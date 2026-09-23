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
#include "FdUtils.hpp"
#include "ResponseQueue.hpp"

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

    // Server kapanırken flushPendingDeletions() bir daha çağrılmayacağı için
    // kalan bekleyen silmeleri de serbest bırak (sızıntı önleme).
    for (size_t i = 0; i < _pendingDeletion.size(); ++i)
        delete _pendingDeletion[i];
    _pendingDeletion.clear();
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

    // stdin tarafı hâlâ epoll'a kayıtlıysa (yazma tamamlanmadan buraya
    // gelindiyse), sadece destructor'daki close()'a güvenmek yerine burada
    // açıkça kaldırıyoruz; aksi halde aynı batch'te bu fd için bekleyen bir
    // event, silinmek üzere olan bu nesneye (ya da bellek yeniden kullanılırsa
    // BAŞKA bir nesneye) yanlışlıkla yönlendirilebilir.
    if (handler->getStdinFd() != -1)
    {
        if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, handler->getStdinFd(), NULL) == -1)
            perror("Epoll dell error (cgi stdin)");
    }

    reapCgiProcess(handler);
    _cgiHandlers.erase(handler);

    // delete'i burada yapmıyoruz: aynı epoll_wait() batch'inde bu handler'a ait
    // başka bir (stdin) event hâlâ _events[] içinde işlenmeyi bekliyor olabilir.
    // Belleği hemen serbest bırakırsak, aynı adres bu batch bitmeden yeni bir
    // CgiHandler için (startCgi çağrısıyla) yeniden kullanılabilir ve o eski
    // event yanlışlıkla yeni nesneye yönlendirilip write()/read() EBADF ile
    // karşılaşabilir (ABA problemi). Gerçek silme flushPendingDeletions() ile
    // batch bitince yapılır.
    _pendingDeletion.push_back(handler);
}

void CgiManager::flushPendingDeletions()
{
    for (size_t i = 0; i < _pendingDeletion.size(); ++i)
        delete _pendingDeletion[i];
    _pendingDeletion.clear();
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
		ResponseQueue::push(_epollFd, client, response, true);
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
			cgiHandler->setStdinBuffer(body, sent);
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

        // Büyük CGI yükleri (örn. 100MB POST → cgi_tester yankısı) 10 sn'nin
        // üzerinde sürebilir ve busy loop esnasında timeout kontrolüne bile
        // ulaşılamayabilir. Bu yüzden 60 sn'lik rahat bir eşik kullanıyoruz;
        // gerçek takılmış bir script için yine de makul bir sürede 504 döner.
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

			Client* client = cgiHandler->getOwner();
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

// BU client'da olduğu gibi Ayrı bir class içerisinde olabilir mi
// Mesela handleReceive  Client.cpp de
void CgiManager::handleCgiReceive(CgiHandler* cgiHandler)
{
	char buffer[65536];
	ssize_t bytesRead = read(cgiHandler->getFd(), buffer, sizeof(buffer));

	if (bytesRead > 0)
	{
		cgiHandler->appendOutput(std::string(buffer, bytesRead));
		return;
	}

	if (bytesRead == -1 && FdUtils::isTransientIoError(errno))
		return;

	finishCgiResponse(cgiHandler);
}

void CgiManager::handleCgiSend(CgiHandler* cgiHandler)
{
	const char* data = cgiHandler->stdinRemainingData();
	size_t      size = cgiHandler->stdinRemainingSize();

	ssize_t written = write(cgiHandler->getStdinFd(), data, size);

	if (written == -1)
	{
		if (FdUtils::isTransientIoError(errno))
			return;
		std::cerr << "CGI stdin write error: " << strerror(errno) << std::endl;
		cgiHandler->closeStdin();
		return;
	}

	cgiHandler->consumeStdinBuffer(static_cast<size_t>(written));

	if (cgiHandler->stdinRemainingSize() == 0)
		cgiHandler->closeStdin();
}
