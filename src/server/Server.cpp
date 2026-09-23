#include "Server.hpp"
#include "ServerConfig.hpp"
#include "RequestParser.hpp"
#include "HttpResponse.hpp"
#include "ResponseBuilder.hpp"
#include "ResponseQueue.hpp"

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <csignal>

Server::Server()
	: _cgiManager(_epollFd, _liveHandlers)
{
	_epollFd = -1;
	_lastTimeoutCheck = std::time(NULL);
}

Server::~Server()
{
	std::set<Client *>::iterator client = _clientSockets.begin();
	std::set<Socket *>::iterator sock = _listenSockets.begin();

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

	// Dangling pointer'ları set<T> den temizliyoruz
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
				registerHandler(sock); // Epoll_Ctl_Add throw atıyor, bu yerden sonra zaten vectore ekleme olduğu için sorun yok
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

void Server::handleClientReceive(Client* client)
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
			handleParsedRequest(client, state);
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
				handleParsedRequest(client, drainState);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		unregisterHandler(client);
	}
}

void Server::handleParsedRequest(Client* client, Client::StreamState state)
{
    if (state == Client::REQUEST_ERROR)
    {
		HttpResponse response = ResponseBuilder::buildErrorResponse(client->getErrorCode(), client->getServerConfig());
		ResponseQueue::push(_epollFd, client, response, true);
    }
    else if (state == Client::TRANSFER_COMPLETE)
    {
		ResponseBuilder::RouteResult routeResult;
		HttpResponse response;

        client->setClientState(Client::PROCESSING_REQUEST);

        std::string scriptPath;
        std::string interpreterPath;
        HttpResponse routeErrorResponse;
        const LocationConfig* matchedLocation = NULL;

        routeResult = ResponseBuilder::routeRequest(client->getRequest(), client->getServerConfig(),
            routeErrorResponse, scriptPath, interpreterPath, matchedLocation);

        if (routeResult == ResponseBuilder::ROUTE_CGI)
        {
            _cgiManager.startCgi(client, scriptPath, interpreterPath);
            return;
        }

        if (routeResult == ResponseBuilder::ROUTE_RESPOND_DIRECTLY)
            response = routeErrorResponse;
        else
            response = ResponseBuilder::build(client->getRequest(), client->getServerConfig());

        ResponseQueue::push(_epollFd, client, response, true);
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
					_cgiManager.finishCgiResponse(static_cast<CgiHandler*>(sock));
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
					handleClientReceive(static_cast<Client*> (sock));
				}
				else if (sock->getType() == EpollHandler::HANDLER_CGI_PIPE)
				{
					_cgiManager.handleCgiReceive(static_cast<CgiHandler*>(sock));
				}
			}
			else if (_events[i].events & EPOLLOUT)
			{
				if (sock->getType() == EpollHandler::HANDLER_CLIENT)
					handleClientSend(static_cast<Client*>(sock), &_events[i]);
				else if (sock->getType() == EpollHandler::HANDLER_CGI_PIPE)
					_cgiManager.handleCgiSend(static_cast<CgiHandler*>(sock));
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
    _cgiManager.checkCgiTimeouts(now);

    // 5 saniye geçtiyse zaman damgasını güncelle ve taramayı yap
    _lastTimeoutCheck = std::time(NULL);
}

void	Server::registerHandler(EpollHandler* socket)
{
	struct epoll_event event;
	std::memset(&event, 0, sizeof(event));

	event.data.ptr = socket;
	event.events = EPOLLIN;



	// EPOLL_CTL_ADD CGI ortak olamaz mı ?

	if (socket->getType() == EpollHandler::HANDLER_CGI_PIPE)
	{
		_cgiManager.registerHandler(static_cast<CgiHandler*>(socket));
		return;  
		// CgiManager epoll_ctl ADD + set insert + _liveHandlers insert yapar
	}

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

	_liveHandlers.insert(socket);
}


void Server::unregisterHandler(EpollHandler* socket)
{
	if (socket->getType() == EpollHandler::HANDLER_CGI_PIPE)
	{
		_cgiManager.unregisterHandler(static_cast<CgiHandler*>(socket));
		return;  // CgiManager epoll_ctl DEL + set erase + _liveHandlers erase yapar
	}

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
	delete socket;
}

