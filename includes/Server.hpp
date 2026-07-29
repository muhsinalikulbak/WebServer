#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Socket.hpp"
#include "EpollHandler.hpp"

#include <string>
#include <map>
#include <sys/epoll.h>
#include <vector>
#include <cstring>
#include <csignal>
#include <set>
#include "ConfigParser.hpp"

class Server
{
private:

    std::set<Socket*>               _listenSockets;
    std::set<Client*>               _clientSockets;
    std::vector<struct epoll_event> _events;            // epoll_wait'in dolduracağı vector
    std::time_t                     _lastTimeoutCheck;
    int                             _epollFd;
    
    Server(const Server& other);
    Server& operator=(const Server& other);

    void    acceptNewConnection(Socket* masterSocket);
    void    handleClientReceive(Client* client, epoll_event *event);  // EPOLLIN: istemciden veri alma
    void    handleClientSend(Client* client, epoll_event *event);     // EPOLLOUT: istemciye veri gönderme
    void    deleteClient(Client* client);
    void    checkExpiredSockets();
    void    registerHandler(EpollHandler* socket);
    
public:

    Server();
    ~Server();

    void    init(const ConfigParser& config); // Config dosyasını alıp socketleri (ip:port) açar
    void    run();

};

#endif
