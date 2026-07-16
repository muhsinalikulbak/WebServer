#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Socket.hpp"
#include <string>
#include <map>
#include <sys/epoll.h>
#include <vector>
#include <cstring>
#include <signal.h>

class Server
{
private:
    // Client'in heap'de olmasının sebebi map kopyalama yaparken eski Client'ler stack de olduğundan hafızadan silinir ve SOCKET KAPANIR.
    Socket                          _masterSocket;
    int                             _epollFd;
    
    std::map<int, Client*>          _clientMap;
    struct epoll_event              _masterEvent;
    std::vector<struct epoll_event> _events; // epoll_wait'in dolduracağı dinamik dizi
    std::time_t                     _lastTimeoutCheck;

public:
    Server(/* args */);
    ~Server();

    
    void    init(int port); // port numarası alır ve masterSocket'e verir  
    void    run();
    bool    acceptNewConnection();
    void    handleClientReceive(int clientFd, epoll_event *event);  // EPOLLIN: istemciden veri alma
    void    handleClientSend(int clientFd, epoll_event *event);     // EPOLLOUT: istemciye veri gönderme
    void    deleteClient(int clientFd);
    void    checkExpiredSockets();
};


#endif // SERVER_HPP
