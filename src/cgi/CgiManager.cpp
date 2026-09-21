#include "CgiManager.hpp"

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>
#include "Client.hpp"
#include "Router.hpp"
#include "ResponseBuilder.hpp"
#include "CgiExecutor.hpp"

CgiManager::CgiManager(int& epollFd, std::set<EpollHandler*>& liveHandlers)
    : _epollFd(epollFd), _liveHandlers(liveHandlers)
{
}

CgiManager::~CgiManager()
{
    std::set<CgiHandler*>::iterator it = _cgiHandlers.begin();
    while (it != _cgiHandlers.end())
    {
        CgiHandler* temp = *it;
        it++;
        reapCgiProcess(temp);
        delete temp;
    }
    _cgiHandlers.clear();
}

void CgiManager::registerHandler(CgiHandler* handler)
{
    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));

    event.data.ptr = handler;
    event.events = EPOLLIN;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, handler->getFd(), &event) == -1)
    {
        throw std::runtime_error(std::string("Error epoll add: ") + strerror(errno));
    }

    _cgiHandlers.insert(handler);
    _liveHandlers.insert(handler);
}

void CgiManager::unregisterHandler(CgiHandler* handler)
{
    _liveHandlers.erase(handler);

    if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, handler->getFd(), NULL) == -1)
    {
        perror("Epoll dell error");
    }

    reapCgiProcess(handler);
    _cgiHandlers.erase(handler);
    delete handler;
}

void CgiManager::reapCgiProcess(CgiHandler* handler)
{
    if (!handler)
        return;

    pid_t pid = handler->getPid();
    if (pid <= 0)
        return;

    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);

    if (result == 0)
    {
        // Child henüz bitmemiş ama biz bu handler'ı kapatıyoruz
        // (timeout ya da client disconnect). Zorla sonlandırıp
        // reap ediyoruz, zombie bırakmamak için.
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    // result == pid: zaten normal şekilde bitmiş ve reap edildi.
    // result == -1 (örn. ECHILD): yapacak bir şey yok, zaten reap edilmiş ya da pid geçersiz.
}

bool CgiManager::peekCgiExitStatus(CgiHandler* cgiHandler, int& status)
{
    pid_t pid = cgiHandler->getPid();

    if (pid <= 0)
        return false;

    pid_t result = waitpid(pid, &status, WNOHANG);
    return (result == pid);
}

void CgiManager::queueResponse(Client* client, const HttpResponse& response, bool throwOnError)
{
    client->setWriteBuffer(response.serialize());

    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.data.ptr = client;
    event.events = EPOLLOUT;

    if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, client->getFd(), &event) == -1)
    {
        if (throwOnError)
            throw std::runtime_error(std::string("Error modifying to EPOLLOUT: ") + strerror(errno));
        else
            std::cerr << "Error modifying client to EPOLLOUT: " << strerror(errno) << std::endl;
    }
}

void CgiManager::registerCgiStdinWrite(CgiHandler* cgiHandler)
{
    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.data.ptr = cgiHandler;
    event.events = EPOLLOUT;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, cgiHandler->getStdinFd(), &event) == -1)
    {
        std::cerr << "Error registering CGI stdin for EPOLLOUT: " << strerror(errno) << std::endl;
        cgiHandler->closeStdin();
    }
}

void CgiManager::startCgi(Client* client,
                          const std::string& scriptPath,
                          const std::string& interpreterPath)
{
	const LocationConfig* location = Router::match(client->getRequest().getPath(), client->getServerConfig());

	CgiExecutor executor;
	if (location)
		executor.buildStandardEnv(client->getRequest(), *location,
		                          client->getServerConfig(), scriptPath);
	executor.setScriptPath(scriptPath);
	executor.setInterpreter(interpreterPath);

	CgiHandler* cgiHandler = executor.execute(client);

	if (!cgiHandler)
	{
		HttpResponse response = ResponseBuilder::buildErrorResponse(500, client->getServerConfig());
		queueResponse(client, response, true);
		return;
	}

	client->setActiveCgi(cgiHandler);

	const std::string& body = client->getRequest().getBody();
	if (!body.empty())
	{
		ssize_t written = write(cgiHandler->getStdinFd(), body.data(), body.size());
		size_t sent = (written > 0) ? static_cast<size_t>(written) : 0;

		if (sent < body.size())
		{
			cgiHandler->setStdinBuffer(body.substr(sent));
			registerCgiStdinWrite(cgiHandler);
		}
		else
			cgiHandler->closeStdin();
	}
	else
		cgiHandler->closeStdin();

	registerHandler(cgiHandler);
}

void CgiManager::checkCgiTimeouts(std::time_t now)
{
    std::set<CgiHandler*>::iterator it = _cgiHandlers.begin();
    std::set<CgiHandler*>::iterator end = _cgiHandlers.end();

    while (it != end)
    {
        CgiHandler* current = *it;
        it++;

        if (now - current->getStartTime() > 10)
        {
            std::cerr << "[Timeout] CGI pid " << current->getPid() << " timed out, killing." << std::endl;

            Client* client = current->getOwner();
            if (client)
            {
                HttpResponse response = ResponseBuilder::buildErrorResponse(504, client->getServerConfig());
                client->setActiveCgi(NULL);
                queueResponse(client, response, false);
            }

            unregisterHandler(current);
        }
    }
}


