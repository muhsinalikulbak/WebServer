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

// epollFd ve liveHandlers referanslarını Server ile paylaşarak CgiManager oluşturur.
CgiManager::CgiManager(int& epollFd, std::set<EpollHandler*>& liveHandlers)
    : _epollFd(epollFd), _liveHandlers(liveHandlers)
{
}

// Kalan tüm CGI handler'ları, bekleyen silmeleri ve reap edilmemiş child'ları temizler.
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

// Yeni bir CGI handler'ı epoll'a (EPOLLIN) ve iç setlere kaydeder.
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

// Bir CGI handler'ını epoll'dan çıkarıp process'ini reap eder; gerçek silme daha sonra yapılır.
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

// unregisterHandler tarafından bekletilen handler'ları, batch bitiminde güvenle serbest bırakır.
void CgiManager::flushPendingDeletions()
{
    for (size_t i = 0; i < _pendingDeletion.size(); ++i)
        delete _pendingDeletion[i];
    _pendingDeletion.clear();
}

// CGI child process'ini SIGKILL ile sonlandırıp reap etmeye çalışır; ölmezse ileride tekrar denenmek üzere kaydeder.
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

// reapCgiProcess'in hemen reap edemediği pid'leri engellemeden tekrar reap etmeyi dener.
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

// Verilen CgiHandler'ın process'i WNOHANG ile çıkış yapmış mı diye bakar; yaptıysa status'ü doldurur.
bool CgiManager::peekCgiExitStatus(CgiHandler* cgiHandler, int& status)
{
    pid_t pid = cgiHandler->getPid();

    if (pid <= 0)
        return false;

    pid_t result = waitpid(pid, &status, WNOHANG);
    return (result == pid);
}

// CGI'nin stdin pipe'ını EPOLLOUT için epoll'a kaydeder; başarısız olursa stdin'i kapatır.
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

// Location, ortam değişkenlerini kurup CGI'yi çalıştırır ve client'a bağlar; body varsa stdin yazımını başlatır.
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

// Belirlenen eşiği (60 sn) aşan CGI'ları 504 döndürüp sonlandırır.
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

// CGI çıktısını parse edip client'a yanıt olarak kuyruğa alır; çıktı boş ve script başarısızsa 502 döner.
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

// CGI'nin stdout pipe'ından gelen veriyi okuyup tampona ekler; EOF'ta yanıtı tamamlar.
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

// İstek body'sinin kalanını CGI'nin stdin pipe'ına yazar; tamamlanınca stdin'i kapatır.
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
