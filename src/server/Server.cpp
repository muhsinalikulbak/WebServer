#include "Server.hpp"
#include "ServerConfig.hpp"
#include "RequestParser.hpp"
#include "HttpResponse.hpp"
#include "ResponseBuilder.hpp"
#include "CgiExecutor.hpp"
#include "Router.hpp"

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <sys/wait.h>
#include <csignal>
#include <sstream>
#include <cstdlib>

Server::Server()
{
	_epollFd = -1;
	_lastTimeoutCheck = std::time(NULL);
}

Server::~Server()
{
	std::set<Client *>::iterator client = _clientSockets.begin();
	std::set<Socket *>::iterator sock = _listenSockets.begin();
	std::set<CgiHandler *>::iterator cgi = _cgiHandlers.begin();


	while (client != _clientSockets.end())
	{
		Client* temp = (*client);
		client++;

		// fd close() yapıldığında otomatik olarak epoll'dan delete edilir
		// O yüzden ekstra epoll_ctl_del yazmaya gerek yoktur
		delete temp;
	}

	while (sock != _listenSockets.end())
	{
		// epoll_ctl(_epollFd, EPOLL_CTL_DEL, (*sock)->getFd(), NULL);
		Socket* temp = *sock;
		sock++;

		delete temp;
	}

	while (cgi != _cgiHandlers.end())
	{
		CgiHandler* temp = *cgi;
		cgi++;
		reapCgiProcess(temp);
		delete temp;
	}

	// Dangling pointer'ları set<T> den temizliyoruz
	_cgiHandlers.clear();
	_clientSockets.clear();
	_listenSockets.clear();

	if (_epollFd != -1)
		close(_epollFd);
}

void Server::init(const ConfigParser& config)
{
	const std::vector<ServerConfig>& servers = config.getServers();
	std::string host;
	int	port = 0;

	// Size parametrese tarihsel bir kalıntı
	// Normalde eskiden bu poll'un kaç adet socket'i yöneteceğini temsil ederdi.
	// Şimdi bu size socket eklendikçe dinamik olarak artıyor.
	// O yüzden parametre sadece 0'dan büyük olmalı başka bir işe yaramıyor.

	_epollFd = epoll_create(1);
	if (_epollFd == -1)
	{
		throw std::runtime_error("Server init failed: epoll_create failed");
	}

	// Bu flag ileride cgi fork attığında kopyalanan epoll fd'yi oto kapatmasını sağlar
	FdUtils::setCloseOnExec(_epollFd);

	for (size_t i = 0; i < servers.size(); i++)
	{
        std::set<std::pair<std::string, int> >::const_iterator it;
		for (it = servers[i].listens.begin(); it != servers[i].listens.end(); ++it)
		{
			host = it->first;
			port = it->second;

			Socket* sock = new Socket(host, port, servers[i]);

			try
			{
				sock->createSocket();
				sock->bindSocket();
				sock->startListening();
				registerHandler(sock);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
				delete sock;
				continue;
			}

			// Dinleme yapacak ip:port aktifleştiriyoruz, dinleyici socket açıyoruz.
			// Epoll_wait çağrısı sonra master socket gelirse bu bir client'ın bağlantı kurmak istemesidir.
			// Artık master socket'e bir bağlantı geldiğinde epoll_wait ile
			// bunu yakalayabileceğiz.
		}

	}

	if (_listenSockets.empty())
		throw std::runtime_error("An error occurred while opening the sockets, or no socket was specified.");

	_events.resize(100); // Burayı dinamik olarak arttırmalı mıyım
}

void Server::acceptNewConnection(Socket* masterSocket)
{
	int clientFd = masterSocket->acceptConnection();
	Client* client = NULL;

	if (clientFd == -1)
	{
		std::cerr << "Error accept: " << strerror(errno) << std::endl;
		return;
	}

	try
	{
		FdUtils::setNonBlocking(clientFd);
		FdUtils::setCloseOnExec(clientFd);
		FdUtils::setTcpNodelay(clientFd);

		client = new Client(clientFd, masterSocket->getServerConfig());
		registerHandler(client);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;

		// Client NULL değilse fd'ye sahiptir ve direk delete ile hem nesneyi hem de fd'yi kapatırız
		// Destructor'daki close(_fd) ile

		if (client)
			delete client;
		else
			close(clientFd);

		// Ama eğer Client NULL ise demek ki new Client(clientFd) satırına gelmeden
		// catch'e düşmüştür yani nesne oluşmamıştır
		// Ama clientFd oluşmuştur o yüzden sadece clientFd close edilir
	}
}

void Server::handleClientReceive(Client* client, epoll_event *event)
{
	try
	{
		Client::StreamState state = client->receiveData();

		client->setClientState(Client::READING_REQUEST);

		if (state == Client::TRANSFER_ERROR || state == Client::PEER_CLOSED)
		{
			// Client bağlantıyı kapattı (EOF) veya hata oluştu
			client->setClientState(Client::CLOSING);
			unregisterHandler(client);
		}
		else
			handleParsedRequest(client, event, state);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		unregisterHandler(client);
	}
}

void Server::handleClientSend(Client* client, epoll_event *event)
{
	try
	{
		Client::StreamState state = client->sendData();

		client->setClientState(Client::SENDING_RESPONSE);

		if (state == Client::TRANSFER_ERROR)
		{
			unregisterHandler(client);
		}
		else if (state == Client::TRANSFER_COMPLETE)
		{
			if (client->isBadRequest()) // Bad request response dönülmüş şimdi kapatılacak.
			{
				unregisterHandler(client);
				return;
			}

			client->setLastActivity(std::time(NULL));
			client->setClientState(Client::WAITING_FOR_REQUEST);
			client->resetParser();
			
			Client::StreamState drainState = client->drainBuffer();

			if (drainState == Client::TRANSFER_INCOMPLETE)
			{
				event->events = EPOLLIN;
				if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, client->getFd(), event) == -1)
				{
					throw std::runtime_error(std::string("Error modifying back to EPOLLIN: ") + strerror(errno));
				}
			}
			else
				handleParsedRequest(client, event, drainState);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		unregisterHandler(client);
	}
}

void Server::handleParsedRequest(Client* client, epoll_event* event, Client::StreamState state)
{
    if (state == Client::REQUEST_ERROR)
    {
		HttpResponse response = ResponseBuilder::buildErrorResponse(client->getErrorCode(), client->getServerConfig());
		queueResponse(client, response, true);
    }
    else if (state == Client::TRANSFER_COMPLETE)
    {
		ResponseBuilder::RouteResult routeResult;
		HttpResponse response;

        client->setClientState(Client::PROCESSING_REQUEST);

        std::string scriptPath;
        std::string interpreterPath;
        HttpResponse routeErrorResponse;

        routeResult = ResponseBuilder::routeRequest(client->getRequest(), client->getServerConfig(),
            routeErrorResponse, scriptPath, interpreterPath);

        if (routeResult == ResponseBuilder::ROUTE_CGI)
        {
            startCgi(client, event, scriptPath, interpreterPath);
            return;
        }

        if (routeResult == ResponseBuilder::ROUTE_RESPOND_DIRECTLY)
            response = routeErrorResponse;
        else
            response = ResponseBuilder::build(client->getRequest(), client->getServerConfig());

        queueResponse(client, response, true);
    }
    // TRANSFER_INCOMPLETE ise hiçbir şey yapma, mevcut event ayarı (EPOLLIN) kalsın
}

void Server::run()
{
	_lastTimeoutCheck = std::time(NULL);

	while (!g_shutdownRequested)
	{

		int activeEvents = epoll_wait(_epollFd, &_events[0], _events.size(), 1000);

		if (activeEvents == -1)
		{
			perror("Epoll wait error");
			continue;
		}

		for (int i = 0; i < activeEvents; i++)
		{
			EpollHandler* sock = static_cast<EpollHandler*>(_events[i].data.ptr);

			if (_liveHandlers.find(sock) == _liveHandlers.end())
				continue; // Bu batch içinde daha önce silinmiş bir handler'a ait bayat event, atla.

			// Client bir istek yollayıp ardından bağlantıyı kapatmak istediğini söyleyebilir.
			// Bu durumda response gitmeli ardından bağlantı kapatılmalı

			if (_events[i].events & EPOLLERR)
			{
				// socket üzerinde hata oluştu(kernel tarafından otomatik set edilir)
	
				if (sock->getType() == EpollHandler::HANDLER_LISTEN)
				{
					perror("Listening socket error");
					unregisterHandler(sock);

					if (_listenSockets.empty())
					{
						throw std::runtime_error("Fatal: All listening sockets closed, server is shutting down.");
					}
				}
				else if (sock->getType() == EpollHandler::HANDLER_CLIENT)
				{
					perror("Client socket error");
					unregisterHandler(sock);
				}
				else if (sock->getType() == EpollHandler::HANDLER_CGI_PIPE)
				{
					perror("CGI pipe error");
					finishCgiResponse(static_cast<CgiHandler*>(sock));
				}
			}
			else if ((_events[i].events & EPOLLIN) || (_events[i].events & EPOLLHUP))
			{
				if (sock->getType() == EpollHandler::HANDLER_LISTEN)
				{
					acceptNewConnection(static_cast<Socket*> (sock));
				}
				else if (sock->getType() == EpollHandler::HANDLER_CLIENT)
				{
					handleClientReceive(static_cast<Client*> (sock), &_events[i]);
				}
				else if (sock->getType() == EpollHandler::HANDLER_CGI_PIPE)
				{
					handleCgiReceive(static_cast<CgiHandler*>(sock));
				}
			}
			else if (_events[i].events & EPOLLOUT)
			{
				if (sock->getType() == EpollHandler::HANDLER_CLIENT)
					handleClientSend(static_cast<Client*>(sock), &_events[i]);
				else if (sock->getType() == EpollHandler::HANDLER_CGI_PIPE)
					handleCgiSend(static_cast<CgiHandler*>(sock));
			}
		}
		checkTimeouts();
	}
}


void Server::checkExpiredSockets(std::time_t now)
{
    std::set<Client*>::iterator it = _clientSockets.begin();
    std::set<Client*>::iterator end = _clientSockets.end();

    while (it != end)
    {
        Client* current = *it;
        it++;

		// buradaki request bekleme flag'i kaldırılabilir, çünkü response üretme aşamasında bir problem çıkıp ya da
		// Uzun sürerek çok fazla beklemeye yol açabilir.
        if (current->getClientState() == Client::WAITING_FOR_REQUEST &&
				now - current->getLastActivity() > 4)
        {
			std::cerr << "[Timeout] Client fd " << current->getFd() << " timed out (keep-alive), closing connection." << std::endl;
			unregisterHandler(current);
        }
    }
}

void Server::checkTimeouts()
{
    std::time_t now = std::time(NULL);

    // Eğer son kontrolden beri 5 saniye geçmediyse HİÇBİR ŞEY YAPMA, direkt dön!
    if (now - _lastTimeoutCheck < 5)
    {
        return;
    }

    checkExpiredSockets(now);
    checkCgiTimeouts(now);

    // 5 saniye geçtiyse zaman damgasını güncelle ve taramayı yap
    _lastTimeoutCheck = std::time(NULL);
}

void Server::checkCgiTimeouts(std::time_t now)
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

bool Server::peekCgiExitStatus(CgiHandler* cgiHandler, int& status)
{
    pid_t pid = cgiHandler->getPid();

    if (pid <= 0)
        return false;

    pid_t result = waitpid(pid, &status, WNOHANG);
    return (result == pid);
}

void	Server::registerHandler(EpollHandler* socket)
{
	struct epoll_event event;
	std::memset(&event, 0, sizeof(event));

	event.data.ptr = socket;
	event.events = EPOLLIN;


	// Pool'a eklenecek soket dinleyen socket'de olabilir,
	// Dinleyen bir socket'in client için açtığı socket'de olabilir.

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, socket->getFd(), &event) == -1)
	{
		throw std::runtime_error(std::string("Error epoll add: ") + strerror(errno));
	}

	if (socket->getType() == EpollHandler::HANDLER_LISTEN)
	{
		_listenSockets.insert(static_cast<Socket*> (socket));
	}
	else if (socket->getType() == EpollHandler::HANDLER_CLIENT)
	{
		_clientSockets.insert(static_cast<Client*> (socket));
	}
	else if (socket->getType() == EpollHandler::HANDLER_CGI_PIPE)
	{
		_cgiHandlers.insert(static_cast<CgiHandler*>(socket));
	}

	_liveHandlers.insert(socket);
}


void Server::unregisterHandler(EpollHandler* socket)
{
	_liveHandlers.erase(socket);

	// Burada close(fd) yerine epoll_ctl_del ile silmemizin sebebi cgi sırasında fd miras alınabilir
	// Ve o process de kapanmadığı için buradaki epoll'dan otomatik olarak silinmeyebilir.
	// O yüzden close(fd) + epoll_ctl_del 'i ekstra olarak ekliyoruz.

	if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, socket->getFd(), NULL) == -1)
	{
		perror("Epoll dell error");
	}

	if (socket->getType() == EpollHandler::HANDLER_LISTEN)
	{
		_listenSockets.erase(static_cast<Socket*> (socket));
	}
	else if (socket->getType() == EpollHandler::HANDLER_CLIENT)
	{
		Client* clientPtr = static_cast<Client*>(socket);
		if (clientPtr->getActiveCgi())
			clientPtr->getActiveCgi()->setOwner(NULL);
		_clientSockets.erase(clientPtr);
	}
	else if (socket->getType() == EpollHandler::HANDLER_CGI_PIPE)
	{
		CgiHandler* cgiHandler = static_cast<CgiHandler*>(socket);
		reapCgiProcess(cgiHandler);
		_cgiHandlers.erase(cgiHandler);
	}
	delete socket;
}

void Server::reapCgiProcess(CgiHandler* handler)
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

void Server::startCgi(Client* client, epoll_event* event,
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

// BU client'da olduğu gibi Ayrı bir class içerisinde olabilir mi
// Mesela handleReceive  Client.cpp de
void Server::handleCgiReceive(CgiHandler* cgiHandler)
{
	char buffer[4096];
	ssize_t bytesRead = read(cgiHandler->getFd(), buffer, sizeof(buffer));

	if (bytesRead > 0)
	{
		cgiHandler->appendOutput(std::string(buffer, bytesRead));
		return;
	}

	if (bytesRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
		return;

	finishCgiResponse(cgiHandler);
}


void Server::finishCgiResponse(CgiHandler* cgiHandler)
{
	Client* client = cgiHandler->getOwner();
	const std::string& rawOutput = cgiHandler->getOutputBuffer();

	std::string::size_type headerEnd = rawOutput.find("\r\n\r\n");
	std::string::size_type separatorLen = 4;

	if (headerEnd == std::string::npos)
	{
		headerEnd = rawOutput.find("\n\n");
		separatorLen = 2;
	}

	std::string headerBlock;
	std::string body;

	if (headerEnd == std::string::npos)
		body = rawOutput;
	else
	{
		headerBlock = rawOutput.substr(0, headerEnd);
		body = rawOutput.substr(headerEnd + separatorLen);
	}

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
				queueResponse(client, errorResponse, false);
			}

			unregisterHandler(cgiHandler);
			return;
		}
	}

	HttpResponse response;
	response.setStatus(200);
	response.setHeader("Content-Type", "text/html");

	if (!headerBlock.empty())
	{
		std::istringstream headerStream(headerBlock);
		std::string line;

		while (std::getline(headerStream, line))
		{
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);
			if (line.empty())
				continue;

			std::string::size_type colonPos = line.find(':');
			if (colonPos == std::string::npos)
				continue;

			std::string key = line.substr(0, colonPos);
			std::string value = line.substr(colonPos + 1);

			while (!value.empty() && value[0] == ' ')
				value.erase(0, 1);

			if (key == "Status")
			{
				int code = std::atoi(value.c_str());
				if (code > 0)
					response.setStatus(code);
			}
			else
				response.setHeader(key, value);
		}
	}

	response.setBody(body);

	if (client)
	{
		client->setActiveCgi(NULL);
		queueResponse(client, response, false);
	}

	unregisterHandler(cgiHandler);
}

void Server::registerCgiStdinWrite(CgiHandler* cgiHandler)
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

void Server::handleCgiSend(CgiHandler* cgiHandler)
{
	const std::string& buffer = cgiHandler->getStdinBuffer();

	ssize_t written = write(cgiHandler->getStdinFd(), buffer.data(), buffer.size());

	if (written == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		std::cerr << "CGI stdin write error: " << strerror(errno) << std::endl;
		cgiHandler->closeStdin();
		return;
	}

	cgiHandler->consumeStdinBuffer(static_cast<size_t>(written));

	if (cgiHandler->getStdinBuffer().empty())
		cgiHandler->closeStdin();
}

// throwOnError: epoll_ctl hatası olduğunda exception fırlatır (true) veya sadece log yazar (false)
// Mevcut koddaki davranışı korumak için: handleParsedRequest/startCgi'de true, checkCgiTimeouts/finishCgiResponse'da fals
void Server::queueResponse(Client* client, const HttpResponse& response, bool throwOnError)
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
