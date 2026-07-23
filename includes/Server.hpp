#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Socket.hpp"
#include "Config.hpp"
#include "EpollHandler.hpp"

#include <string>
#include <map>
#include <sys/epoll.h>
#include <vector>
#include <cstring>
#include <signal.h>
#include <set>


class Server
{
private:

    std::set<Socket*>               _listenSockets;
    std::set<Client*>               _clientSockets;


    // İstemci haritası: clientFd -> Client*

    // Config nesnesi
    Config                          _config;
    int                             _epollFd;
    
    std::vector<struct epoll_event> _events; // epoll_wait'in dolduracağı dinamik dizi
    std::time_t                     _lastTimeoutCheck;

public:
    Server(/* args */);
    ~Server();

    void    init(const Config& config); // port numarası alır ve masterSocket'e verir  
    void    run();
    void    acceptNewConnection(Socket* masterSocket);
    void    handleClientReceive(Client* client, epoll_event *event);  // EPOLLIN: istemciden veri alma
    void    handleClientSend(Client* client, epoll_event *event);     // EPOLLOUT: istemciye veri gönderme
    void    deleteClient(Client* client);
    void    checkExpiredSockets();

};

#endif