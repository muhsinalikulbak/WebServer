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
#include <set>
#include "Config.hpp"

class Server
{
private:
    std::map<int, Socket*>          _listenSockets; 

    // İstemci haritası: clientFd -> Client*
    std::map<int, Client*>          _clientMap;

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
    bool    acceptNewConnection(Socket* masterSocket);
    void    handleClientReceive(int clientFd, epoll_event *event);  // EPOLLIN: istemciden veri alma
    void    handleClientSend(int clientFd, epoll_event *event);     // EPOLLOUT: istemciye veri gönderme
    void    deleteClient(int clientFd);
    void    checkExpiredSockets();
    bool    isListeningFd(int fd) const;

};

#endif