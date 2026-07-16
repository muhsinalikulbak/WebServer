#include "Server.hpp"

Server::Server()
{
	_epollFd = -1;
	// Sunucu portu field'mı olmalı cluster için sorun yaratır mı
	// Yoksa sadece init de alıp kullanmalı mı
}

Server::~Server()
{
	std::map<int, Client *>::iterator it = _clientMap.begin();

	while (it != _clientMap.end())
	{
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, it->first, NULL);
		delete it->second;
		it++;
	}

	_clientMap.clear();

	if (_epollFd != -1)
		close(_epollFd);
}

void Server::init(int port)
{
	_masterSocket.createSocket();
	_masterSocket.bindSocket(port);
	_masterSocket.startListening();

// Bu flag ileride cgi fork attığında kopyalanan epoll fd'yi oto kapatmasını sağlar
	_epollFd = epoll_create1(EPOLL_CLOEXEC); 
	if (_epollFd == -1)
	{
		perror("epoll error");
	}

	std::memset(&_masterEvent, 0, sizeof(_masterEvent));

	// Master sockete yeni bir biri bağlantığında bu bir okuma/connect olayıdır.
	// Bu yüzden EPOLLIN ile master sockete epoll_ctl ile ekliyoruz.
	_masterEvent.events = EPOLLIN;

	_masterEvent.data.fd = _masterSocket.getFd();

	// (master socket) için içerideki veri akışı, yeni bir istemcinin (client)
	// kapıyı çalıp bağlanmak istemesi demektir. Master socket'i epoll'a
	// ekliyoruz. Artık master socket'e bir bağlantı geldiğinde epoll_wait ile
	// bunu yakalayabileceğiz.
	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _masterSocket.getFd(), &_masterEvent) == -1)
	{
		perror("epoll ctrl error");
	}

	_events.resize(100);
}

bool Server::acceptNewConnection()
{
	int clientFd = _masterSocket.acceptConnection();

	if (clientFd == -1)
	{
		perror("Error accept");
		return false;
	}

	int flags = fcntl(clientFd, F_GETFL, 0);
	int opt = 1;

	fcntl(clientFd, F_SETFL, flags | O_NONBLOCK); // fd'yi non blocking yapar
	setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)); // NAGLE ALGORİTMASINI KAPAT

	struct epoll_event event;
	event.data.fd = clientFd;
	event.events = EPOLLIN; // Bu yeni client'den gelen istekleri dinlemek istiyorum

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1)
	{
		perror("Error epoll add");
		close(clientFd);
		return false;
	}

	_clientMap[clientFd] = new Client(clientFd);
	return true;
}

void Server::handleClientReceive(int clientFd, epoll_event *event) 
{
	Client *client = _clientMap[clientFd];
	StreamState state = client->receiveData();

	client->setClientState(READING_REQUEST);

	if (state == TRANSFER_ERROR || state == PEER_CLOSED)
	{
		// Client bağlantıyı kapattı (EOF) veya hata oluştu
		client->setClientState(CLOSING);
		deleteClient(clientFd);
	} 
	else if (state == TRANSFER_COMPLETE)
	{
		client->setClientState(PROCESSING_REQUEST);

		std::string dummyResponse = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: keep-alive\r\n\r\nHello World!!\n";
		client->appendToWriteBuffer(dummyResponse);

		event->events = EPOLLOUT;

		if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, event) == -1)
		{
			perror("Error modifying to EPOLLOUT");
			deleteClient(clientFd);
		}
		client->clearReadBuffer();
	}

}

void Server::handleClientSend(int clientFd, epoll_event *event)
{
	Client *client = _clientMap[clientFd];
	StreamState state = client->sendData();

	client->setClientState(SENDING_RESPONSE);

	if (state == TRANSFER_ERROR)
	{
		// Gönderim hatası, bağlantıyı kopar
		deleteClient(clientFd);
	} 
	else if (state == TRANSFER_COMPLETE)
	{
		client->setLastActivity(time(NULL));
		client->setClientState(WAITING_FOR_REQUEST);

		// Response başarıyla tamamen gönderildi!
		// Keep-Alive aktif olduğu için soketi kapatmıyoruz, yeni istekler için
		// tekrar EPOLLIN moduna alıyoruz.
		event->events = EPOLLIN;
		if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, event) == -1)
		{
			perror("Error modifying back to EPOLLIN");
			deleteClient(clientFd);
		}
	}

}

void Server::deleteClient(int clientFd)
{
	// Delete ederken epoll_event nesnesine gerek yok sadece fd ile epoll dan
	// silebilir
	if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, NULL) == -1)
	{
		perror("error epoll dell");
	}
	close(clientFd);
	delete _clientMap[clientFd];
	_clientMap.erase(clientFd);
}

void Server::run()
{
	int currentFd = 0;
	_lastTimeoutCheck = time(NULL);

	while (true)
	{

		int activeEvents = epoll_wait(_epollFd, &_events[0], _events.size(), 1000);

		if (activeEvents == -1)
		{
			perror("epoll wait");
			// throw mu fırlatmalıyım
		}

		for (int i = 0; i < activeEvents; i++)
		{
			currentFd = _events[i].data.fd;

			if (_events[i].events & (EPOLLERR | EPOLLHUP))
			{
				deleteClient(currentFd);
				// socket üzerinde hata oluştu(kernel tarafından otomatik set edilir)
				// Ya da Bağlantı koptu ya da /hang up (kernel tarafından otomatik set
				// edilir.)
			} 
			else if (_events[i].events & EPOLLIN)
			{
				if (currentFd == _masterSocket.getFd())
				{
					// Yeni client'i epoll'a ekliyoruz.
					acceptNewConnection();
				} 
				else
				{
					// Var olan client'dan request gelmiş
					handleClientReceive(currentFd, &_events[i]);
				}
			} 
			else if (_events[i].events & EPOLLOUT)
			{
				handleClientSend(currentFd, &_events[i]);
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


    std::map<int, Client*>::iterator it = _clientMap.begin();
    std::map<int, Client*>::iterator end = _clientMap.end();

    while (it != end)
    {
        std::map<int, Client*>::iterator current = it;
        it++;

        if (current->second->getClientState() == WAITING_FOR_REQUEST &&	 
				now - current->second->getLastActivity() > 5)
        {
            std::cout << "[Timeout] Client Fd " << current->first << " zaman aşımına uğradı, kapatılıyor." << std::endl;
            deleteClient(current->first); 
        }
    }

    // 5 saniye geçtiyse zaman damgasını güncelle ve taramayı yap
    _lastTimeoutCheck = now;
}