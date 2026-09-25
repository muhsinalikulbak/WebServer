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
#include "CgiResponseParser.hpp"
#include "ResponseQueue.hpp"

// Creates a CgiManager sharing references to epollFd and liveHandlers with the Server.
CgiManager::CgiManager(int& epollFd, std::set<EpollHandler*>& liveHandlers)
    : _epollFd(epollFd), _liveHandlers(liveHandlers)
{
}

// Cleans up all remaining CGI handlers, pending removals, and unreaped child processes.
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

    for (size_t i = 0; i < _pendingDeletion.size(); ++i)
        delete _pendingDeletion[i];
    _pendingDeletion.clear();

    for (size_t i = 0; i < _pendingReap.size(); ++i)
    {
        int status;
        waitpid(_pendingReap[i], &status, 0);
    }
}

// Registers a new CGI handler with epoll for EPOLLIN and adds it to the internal sets.
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

// Unregisters a CGI handler from epoll and reaps its process; the actual deletion happens later.
void CgiManager::unregisterHandler(CgiHandler* handler)
{
    _liveHandlers.erase(handler);

    if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, handler->getFd(), NULL) == -1)
    {
        perror("Epoll dell error");
    }

    if (handler->getStdinFd() != -1)
    {
        if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, handler->getStdinFd(), NULL) == -1)
            perror("Epoll dell error (cgi stdin)");
    }

    reapCgiProcess(handler);
    _cgiHandlers.erase(handler);

    _pendingDeletion.push_back(handler);
}

// Safely releases handlers deferred by unregisterHandler when the batch is complete.
void CgiManager::flushPendingDeletions()
{
    for (size_t i = 0; i < _pendingDeletion.size(); ++i)
        delete _pendingDeletion[i];
    _pendingDeletion.clear();
}

// Attempts to kill and reap the CGI child process with SIGKILL; if it remains alive, schedules another attempt.
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
        kill(pid, SIGKILL);

        result = waitpid(pid, &status, WNOHANG);
        if (result == 0)
            _pendingReap.push_back(pid);
    }
}

// Retries reaping pids that reapCgiProcess could not reap immediately, without blocking.
void CgiManager::reapPendingKills()
{
    std::vector<pid_t>::iterator it = _pendingReap.begin();
    while (it != _pendingReap.end())
    {
        int status;
        pid_t result = waitpid(*it, &status, WNOHANG);
        if (result != 0)
            it = _pendingReap.erase(it);
        else
            ++it;
    }
}

// Checks with WNOHANG whether the process for the given CgiHandler has exited and, if so, stores its status.
bool CgiManager::peekCgiExitStatus(CgiHandler* cgiHandler, int& status)
{
    pid_t pid = cgiHandler->getPid();

    if (pid <= 0)
        return false;

    pid_t result = waitpid(pid, &status, WNOHANG);
    return (result == pid);
}

// Registers the CGI stdin pipe with epoll for EPOLLOUT; closes stdin if registration fails.
void CgiManager::registerCgiStdinWrite(CgiHandler* cgiHandler)
{
    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.data.ptr = cgiHandler;
    event.events = EPOLLOUT;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, cgiHandler->getStdinFd(), &event) == -1)
    {
        std::cerr << "Error registering CGI stdin for EPOLLOUT: " << strerror(errno) << std::endl;
        cgiHandler->closeStdin(_epollFd);
    }
}

// Sets up the location and environment, runs the CGI, and associates it with the client; starts writing stdin if a body exists.
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
		ResponseQueue::push(_epollFd, client, response, true);
		return;
	}

	client->setActiveCgi(cgiHandler);

	const std::string& body = client->getRequest().getBody();
	if (!body.empty())
	{
		cgiHandler->setStdinOffset(0);
		registerCgiStdinWrite(cgiHandler);
	}
	else
		cgiHandler->closeStdin(_epollFd);

	registerHandler(cgiHandler);
}

// Terminates CGI processes that exceed the 60-second limit and returns 504.
void CgiManager::checkCgiTimeouts(std::time_t now)
{
    reapPendingKills();

    std::set<CgiHandler*>::iterator it = _cgiHandlers.begin();
    std::set<CgiHandler*>::iterator end = _cgiHandlers.end();

    while (it != end)
    {
        CgiHandler* current = *it;
        it++;

        if (now - current->getStartTime() > 60)
        {
            std::cerr << "[Timeout] CGI pid " << current->getPid() << " timed out, killing." << std::endl;

            Client* client = current->getOwner();
            if (client)
            {
                HttpResponse response = ResponseBuilder::buildErrorResponse(504, client->getServerConfig());
                client->setActiveCgi(NULL);
                ResponseQueue::push(_epollFd, client, response, false);
            }

            unregisterHandler(current);
        }
    }
}

// Parses CGI output and queues it as the client response; returns 502 if the output is empty and the script failed.
void CgiManager::finishCgiResponse(CgiHandler* cgiHandler)
{
	Client* client = cgiHandler->getOwner();
	const std::string& rawOutput = cgiHandler->getOutputBuffer();

	if (rawOutput.empty())
	{
		int status = 0;
		bool exited = peekCgiExitStatus(cgiHandler, status);
		bool failed = !exited || (WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status);

		if (failed)
		{
			std::cerr << "[CGI] Script produced no output and did not exit cleanly (pid "
					   << cgiHandler->getPid() << ")." << std::endl;

			if (client)
			{
				HttpResponse errorResponse = ResponseBuilder::buildErrorResponse(502, client->getServerConfig());
				client->setActiveCgi(NULL);
				ResponseQueue::push(_epollFd, client, errorResponse, false);
			}

			unregisterHandler(cgiHandler);
			return;
		}
	}

	HttpResponse response = CgiResponseParser::parse(rawOutput);

	if (client)
	{
		client->setActiveCgi(NULL);
		ResponseQueue::push(_epollFd, client, response, false);
	}

	unregisterHandler(cgiHandler);
}

// Reads data from the CGI stdout pipe into the buffer and completes the response at EOF.
void CgiManager::handleCgiReceive(CgiHandler* cgiHandler)
{
	char buffer[65536];
	ssize_t bytesRead = read(cgiHandler->getFd(), buffer, sizeof(buffer));

	if (bytesRead > 0)
	{
		cgiHandler->appendOutput(std::string(buffer, bytesRead));
		return;
	}

	if (bytesRead == 0)
	{
		finishCgiResponse(cgiHandler);
		return;
	}

	if (bytesRead < 0)
		return;
}

// Writes the remaining request body to the CGI stdin pipe and closes stdin when finished.
void CgiManager::handleCgiSend(CgiHandler* cgiHandler)
{
	if (cgiHandler->getStdinFd() == -1)
		return;

	const char* data = cgiHandler->stdinRemainingData();
	size_t      size = cgiHandler->stdinRemainingSize();

	ssize_t written = write(cgiHandler->getStdinFd(), data, size);

	if (written < 0)
	{
		return;
	}

	if (written == 0)
	{
		std::cerr << "CGI stdin write returned 0, closing stdin" << std::endl;
		cgiHandler->closeStdin(_epollFd);
		return;
	}

	if (written > 0)
	{
		cgiHandler->consumeStdinBuffer(static_cast<size_t>(written));

		if (cgiHandler->stdinRemainingSize() == 0)
			cgiHandler->closeStdin(_epollFd);
	}
}
