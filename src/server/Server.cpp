#include "Server.hpp"
#include "ServerConfig.hpp"
#include <cstring>
#include <cerrno>

Server::Server()
{
	_epollFd = -1;
	_lastTimeoutCheck = time(NULL);
}

Server::~Server()
{
	std::set<Client *>::iterator client = _clientSockets.begin();
	std::set<Socket *>::iterator sock = _listenSockets.begin();


	while (client != _clientSockets.end())
	{
		Client* temp = (*client);
		client++;

		deleteClient(temp);
	}
	
	while (sock != _listenSockets.end())
	{
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, (*sock)->getFd(), NULL);
		Socket* temp = *sock;
		sock++;

		_listenSockets.erase(temp);
		delete temp;
	}


	_clientSockets.clear();
	_listenSockets.clear();

	if (_epollFd != -1)
		close(_epollFd);
}

void Server::init(const Config& config)
{
	const std::vector<ServerConfig>& servers = config.getServers();
	std::set<int> openedPorts;
	int currentPort = 0;

	// Bu flag ileride cgi fork attığında kopyalanan epoll fd'yi oto kapatmasını sağlar
	_epollFd = epoll_create1(EPOLL_CLOEXEC); 
	if (_epollFd == -1)
	{
		perror("epoll error");
		throw std::runtime_error("Server init failed: epoll_create1 failed");
	}

	for (size_t i = 0; i < servers.size(); i++)
	{
		currentPort = servers[i].port;

		// Configde aynı port gelirse tekrar bind etmesin
		if (openedPorts.find(currentPort) != openedPorts.end())
			continue;

		Socket* sock = new Socket(AF_INET, SOCK_STREAM);
		
		try
		{
			sock->createSocket();
			sock->bindSocket(currentPort);
			sock->startListening();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			delete sock;
			continue;
		}


		epoll_event masterEvent;
		std::memset(&masterEvent, 0, sizeof(masterEvent));

		// Master sockete yeni bir biri bağlantığında bu bir okuma/connect olayıdır.
		// Bu yüzden EPOLLIN ile master sockete epoll_ctl ile ekliyoruz.
		masterEvent.events = EPOLLIN;
		masterEvent.data.ptr = sock;

		// (master socket) için içerideki veri akışı, yeni bir istemcinin (client)
		// kapıyı çalıp bağlanmak istemesi demektir. Master socket'i epoll'a
		// ekliyoruz. Artık master socket'e bir bağlantı geldiğinde epoll_wait ile
		// bunu yakalayabileceğiz.

		if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, sock->getFd(), &masterEvent) == -1)
		{
			perror("epoll ctrl error");
			delete sock;
			continue;
		}

		// epoll_ctl başarılı olduktan sonra port'u openedPorts'a ekle
		openedPorts.insert(currentPort);
		_listenSockets.insert(sock);
	}
	_events.resize(100);
}

void Server::acceptNewConnection(Socket* masterSocket)
{
	try
	{
		int clientFd = masterSocket->acceptConnection();

		if (clientFd == -1)
		{
			throw std::runtime_error(std::string("Error accept: ") + strerror(errno));
		}

		int flags = fcntl(clientFd, F_GETFL, 0);
		if (flags == -1)
		{
			close(clientFd);
			throw std::runtime_error(std::string("Error fcntl F_GETFL: ") + strerror(errno));
		}
		int opt = 1;

		if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1) // fd'yi non blocking yapar
		{
			close(clientFd);
			throw std::runtime_error(std::string("Error fcntl F_SETFL: ") + strerror(errno));
		}
		if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) // NAGLE ALGORİTMASINI KAPAT
		{
			close(clientFd);
			throw std::runtime_error(std::string("Error setsockopt TCP_NODELAY: ") + strerror(errno));
		}

		Client* client = new Client(clientFd);
		struct epoll_event event;
		event.data.ptr = client;
		event.events = EPOLLIN; // Bu yeni client'den gelen istekleri dinlemek istiyorum

		if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1)
		{
			close(clientFd);
			delete client;
			throw std::runtime_error(std::string("Error epoll add: ") + strerror(errno));
		}
		_clientSockets.insert(client);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

void Server::handleClientReceive(Client* client, epoll_event *event)
{
	try
	{
		StreamState state = client->receiveData();

		client->setClientState(READING_REQUEST);

		if (state == TRANSFER_ERROR || state == PEER_CLOSED)
		{
			// Client bağlantıyı kapattı (EOF) veya hata oluştu
			client->setClientState(CLOSING);
			deleteClient(client);
		}
		else if (state == TRANSFER_COMPLETE)
		{
			// Burası tekrar read'e düşebilir  / Chunked veya body okuması gerekebilir
			client->setClientState(PROCESSING_REQUEST);

			std::string dummyResponse = "HTTP/1.1 200 OK\r\nContent-Length: 14\r\nConnection: keep-alive\r\n\r\nHello World!!\n";
			client->appendToWriteBuffer(dummyResponse);

			event->events = EPOLLOUT;

			if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, client->getFd(), event) == -1)
			{
				throw std::runtime_error(std::string("Error modifying to EPOLLOUT: ") + strerror(errno));
			}
			client->clearReadBuffer();
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		deleteClient(client);
	}
}

void Server::handleClientSend(Client* client, epoll_event *event)
{
	try
	{
		StreamState state = client->sendData();

		client->setClientState(SENDING_RESPONSE);

		if (state == TRANSFER_ERROR)
		{
			// Gönderim hatası, bağlantıyı kopar
			deleteClient(client);
		}
		else if (state == TRANSFER_COMPLETE)
		{
			client->setLastActivity(time(NULL));
			client->setClientState(WAITING_FOR_REQUEST);

			// Response başarıyla tamamen gönderildi!
			// Keep-Alive aktif olduğu için soketi kapatmıyoruz, yeni istekler için
			// tekrar EPOLLIN moduna alıyoruz.
			event->events = EPOLLIN;
			if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, client->getFd(), event) == -1)
			{
				throw std::runtime_error(std::string("Error modifying back to EPOLLIN: ") + strerror(errno));
			}
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		deleteClient(client);
	}
}

void Server::deleteClient(Client* client)
{
	// Delete ederken epoll_event nesnesine gerek yok sadece fd ile epoll dan
	// silebilir
	if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, client->getFd(), NULL) == -1)
	{
		perror("error epoll dell");
	}
	_clientSockets.erase(client);
	delete client;
}

void Server::run()
{
	EpollHandler* sock = NULL;
	_lastTimeoutCheck = time(NULL);

	while (true)
	{

		int activeEvents = epoll_wait(_epollFd, &_events[0], _events.size(), 1000);

		if (activeEvents == -1)
		{
			perror("epoll wait");
			continue;
		}

		for (int i = 0; i < activeEvents; i++)
		{
			sock = static_cast<EpollHandler*>(_events[i].data.ptr);

			if (_events[i].events & (EPOLLERR | EPOLLHUP))
			{
				if (sock->isListening())
				{
					perror("Listening socket error");
					epoll_ctl(_epollFd, EPOLL_CTL_DEL, sock->getFd(), NULL);
					_listenSockets.erase(static_cast<Socket*> (sock));
					delete sock;

					if (_listenSockets.empty())
					{
						throw std::runtime_error("Fatal: All listening sockets closed, server is shutting down.");
					}
				}
				else
					deleteClient(static_cast<Client*>(sock));
				// socket üzerinde hata oluştu(kernel tarafından otomatik set edilir)
				// Ya da Bağlantı koptu ya da /hang up (kernel tarafından otomatik set
				// edilir.)
			} 
			else if (_events[i].events & EPOLLIN)
			{
				if (sock->isListening())
				{
					// Yeni client'i epoll'a ekliyoruz.
					acceptNewConnection(static_cast<Socket*> (sock));
				}
				else
				{
					// Var olan client'dan request gelmiş
					handleClientReceive(static_cast<Client*> (sock), &_events[i]);
				}
			} 
			else if (_events[i].events & EPOLLOUT)
			{
				handleClientSend(static_cast<Client*> (sock), &_events[i]);
			}
		}
		checkExpiredSockets();
	}
}


void Server::checkExpiredSockets()
{
    std::time_t now = time(NULL);

    // Eğer son kontrolden beri 5 saniye geçmediyse HİÇBİR ŞEY YAPMA, direkt dön!
    if (now - _lastTimeoutCheck < 5)
    {
        return;
    }


    std::set<Client*>::iterator it = _clientSockets.begin();
    std::set<Client*>::iterator end = _clientSockets.end();

    while (it != end)
    {
        Client* current = *it;
        it++;

		// buradaki request bekleme flag'i kaldırılabilir, çünkü response üretme aşamasında bir problem çıkıp ya da
		// Uzun sürerek çok fazla beklemeye yol açabilir.
		
        if (current->getClientState() == WAITING_FOR_REQUEST &&	 
				now - current->getLastActivity() > 5)
        {
            std::cout << "[Timeout] Client Fd " << current->getFd() << " zaman aşımına uğradı, kapatılıyor." << std::endl;
            deleteClient(current); 
        }
    }

    // 5 saniye geçtiyse zaman damgasını güncelle ve taramayı yap
    _lastTimeoutCheck = time(NULL);
}

