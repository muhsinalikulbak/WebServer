#include "Server.hpp"
#include "Socket.hpp"

Server::Server(/* args */)
{
    
}

Server::~Server()
{
    std::map <int, Client*>::iterator it = _clientMap.begin();
    
    while (it != _clientMap.end())
    {
        delete it->second;
        it++;
    }
}

void Server::init(int port)
{
    _masterSocket.createSocket();
    _masterSocket.bindSocket(port);
    _masterSocket.startListening();

    _epollFd = epoll_create1(EPOLL_CLOEXEC); // Bu flag ileride cgi fork attığında kopyalanan epoll fd'yi oto kapatmasını sağlar
    if (_epollFd == -1)
    {
        perror("epoll error");
    }

    std::memset(&_masterEvent, 0, sizeof(_masterEvent));
    
    // Master sockete yeni bir biri bağlantığında bu bir okuma/connect olayıdır. Bu yüzden EPOLLIN ile master sockete epoll_ctl ile ekliyoruz.
    _masterEvent.events = EPOLLIN;
    
    _masterEvent.data.fd = _masterSocket.getFd();

    // (master socket) için içerideki veri akışı, yeni bir istemcinin (client) kapıyı çalıp bağlanmak istemesi demektir.
    // Master socket'i epoll'a ekliyoruz. Artık master socket'e bir bağlantı geldiğinde epoll_wait ile bunu yakalayabileceğiz.
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

    struct epoll_event event;
    event.data.fd = clientFd;
    event.events = EPOLLIN; // Bu yeni client'den gelen istekleri dinlemek istiyorum

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1)
    {
        perror("Error epoll add");
        return false;
        // Burada throw fırlatıp program mı sonlanmalı yoksa
        // Sonraki client'lara devam mı etmeli.
    }
    _clientMap[clientFd] = new Client(clientFd);
    return true;
}

void Server::handleClientData(int clientFd)
{

}

void Server::run()
{
    while (true)
    {
        int activeEvents = epoll_wait(_epollFd, &_events[0], _events.size(), -1);

        if (activeEvents == -1)
        {
            perror("epoll wait");
            // throw fırlat
        }

        for (int i = 0; i < activeEvents; i++)
        {
            if (_events[i].events & (EPOLLERR || EPOLLHUP))
            {
                if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, _events[i].data.fd, &_events[i]) == -1)
                {
                    perror("error epoll dell");
                }
                // socket üzerinde hata oluştu(kernel tarafından otomatik set edilir)
                // Ya da Bağlantı koptu ya da /hang up (kernel tarafından otomatik set edilir.)
            }
            else if (_events[i].events & EPOLLIN)
            {
                if (_events[i].data.fd == _masterSocket.getFd())
                {
                    // Yeni client'i epoll'a ekliyoruz.
                    acceptNewConnection();
                }
                else
                {
                    // Var olan client'dan request gelmiş
                }
            }
            else if (_events[i].events & EPOLLOUT)
            {
                // Yazma işlemi için için buffer'a veri yazılır
            }
        }
    }
}
