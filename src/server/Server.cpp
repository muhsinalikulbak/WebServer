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
    
    _masterEvent.events = EPOLLIN;
    _masterEvent.data.fd = _masterSocket.getFd();

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _masterSocket.getFd(), &_masterEvent) == -1)
    {
        perror("epoll ctrl error");
    }

    _events.resize(100);

}

void Server::run()
{

}
