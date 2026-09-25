#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Socket.hpp"
#include "EpollHandler.hpp"
#include "HttpResponse.hpp"
#include "CgiManager.hpp"

#include <string>
#include <map>
#include <sys/epoll.h>
#include <vector>
#include <cstring>
#include <csignal>
#include <set>
#include "ConfigParser.hpp"

extern volatile sig_atomic_t g_shutdownRequested;

class Server
{
private:

    std::set<Socket*>               _listenSockets;
    std::set<Client*>               _clientSockets;
    std::set<EpollHandler*>         _liveHandlers;

    std::vector<struct epoll_event> _events;
    std::time_t                     _lastTimeoutCheck;
    int                             _epollFd;
    CgiManager                      _cgiManager;
    
    Server(const Server& other);
    Server& operator=(const Server& other);

    void    acceptNewConnection(Socket* masterSocket);
    void    handleClientReceive(Client* client);
    void    handleClientSend(Client* client, epoll_event *event);
    void    checkExpiredSockets(std::time_t now);
    void    checkTimeouts();
    void    registerHandler(EpollHandler* socket);
    void    unregisterHandler(EpollHandler* socket);
    void    handleParsedRequest(Client* client, Client::StreamState state);

    
public:

    Server();
    ~Server();

    void    init(const ConfigParser& config);
    void    run();

};

#endif
