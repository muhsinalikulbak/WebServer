#include "Server.hpp"
#include "ServerConfig.hpp"
#include <cstring>
#include <cerrno>

Server::Server()
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
	int port = 0;
	std::string host; 
	
	// Size parametrese tarihsel bir kalıntı
	// Normalde eskiden bu poll'un kaç adet socket'i yöneteceğini temsil ederdi.
	// Şimdi bu size socket eklendikteç dinamik olarak artıyor.
	// O yüzden parametre sadece 0'dan büyük olmalı başka bir işe yaramıyor.

	_epollFd = epoll_create(1);
	if (_epollFd == -1)
	{
		throw std::runtime_error("Server init failed: epoll_create failed");
	}

	// Bu flag ileride cgi fork attığında kopyalanan epoll fd'yi oto kapatmasını sağlar
	fcntl(_epollFd, F_SETFL, EPOLL_CLOEXEC);

	for (size_t i = 0; i < servers.size(); i++)
	{
		for (size_t j = 0; j < servers[i].listens.size(); j++)
		{
			host = servers[i].listens[j].first;
			port = servers[i].listens[j].second;

			Socket* sock = new Socket(AF_INET, SOCK_STREAM);
			
			try
			{
				sock->createSocket();
				sock->bindSocket(host, port);
				sock->startListening();	
				sock->setServerConfig(&servers[i]);
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

			_listenSockets.insert(sock);
		}

	}

	if (_listenSockets.empty())
		throw std::runtime_error("An error occurred while opening the sockets, or no socket was specified.");
	
	_events.resize(100); // Burayı dinamik olarak arttırmalı mıyım / artırmalıyım
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

		if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) // fd'yi non blocking yapar
		{
			close(clientFd);
			throw std::runtime_error(std::string("Error fcntl F_SETFL: ") + strerror(errno));
		}

		int opt = 1;
		if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) // NAGLE ALGORİTMASINI KAPAT
		{
			close(clientFd);
			throw std::runtime_error(std::string("Error setsockopt TCP_NODELAY: ") + strerror(errno));
		}

		Client* client = new Client(clientFd);
		registerHandler(client);

		client->setServerConfig(masterSocket->getServerConfig());
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

			// Buradan emin miyiz ?
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
			client->setLastActivity(std::time(NULL));
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
		std::cerr << "Epoll dell error : " << strerror(errno) << std::endl;
	}
	_clientSockets.erase(client);
	delete client;
}

void Server::run()
{
	_lastTimeoutCheck = std::time(NULL);

	while (true)
	{

		int activeEvents = epoll_wait(_epollFd, &_events[0], _events.size(), 1000);
	
		if (activeEvents == -1)
		{
			std::cerr << "Epoll wait error : " << strerror(errno) << std::endl;
			continue;
		}

		for (int i = 0; i < activeEvents; i++)
		{
			EpollHandler* sock = static_cast<EpollHandler*>(_events[i].data.ptr);

			if (_events[i].events & (EPOLLERR | EPOLLHUP))
			{
				// socket üzerinde hata oluştu(kernel tarafından otomatik set edilir)
				// Ya da Bağlantı koptu ya da /hang up (kernel tarafından otomatik set
				// edilir.)

				if (sock->isListening())
				{
					std::cerr << "Listening socket error : " << strerror(errno) << std::endl;
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
				// Burada close(fd) yerine epoll_ctl_del ile silmemizin sebebi cgi sırasında fd miras alınabilir
				// Ve o process de kapanmadığı için buradaki epoll'dan otomatik olarak silinmeyebilir.
				// O yüzden close(fd) + epoll_ctl_del 'i ekstra olarak ekliyoruz.
				
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
    std::time_t now = std::time(NULL);

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
			std::cerr << "[Timeout] Client fd " << current->getFd() << " timed out (keep-alive), closing connection." << std::endl;            
			deleteClient(current); 
        }
    }

    // 5 saniye geçtiyse zaman damgasını güncelle ve taramayı yap
    _lastTimeoutCheck = std::time(NULL);
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
		delete socket;
		throw std::runtime_error(std::string("Error epoll add: ") + strerror(errno));
	}
}
