#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Socket.hpp"
#include "EpollHandler.hpp"
#include "CgiHandler.hpp"

#include <string>
#include <map>
#include <sys/epoll.h>
#include <vector>
#include <cstring>
#include <csignal>
#include <set>
#include "ConfigParser.hpp"

// extern: Bu değişkenin main.cpp'de tanımlandığını bildirir
// volatile sig_atomic_t: Signal-safe global shutdown flag
// Server ve diğer sınıfların bu değişkene erişebilmesi için extern kullanılır
extern volatile sig_atomic_t g_shutdownRequested;

class Server
{
private:

    // Setleri set<EpollHandler*> olarak ayarlayabilirim
    // Bu sayede kodda  bazı yerlerdeki static_cast<T> lere ihtiyacımız kalmaz.
    std::set<Socket*>               _listenSockets;
    std::set<Client*>               _clientSockets;
    std::set<CgiHandler*>           _cgiHandlers;

    std::vector<struct epoll_event> _events;            // epoll_wait'in dolduracağı vector
    std::time_t                     _lastTimeoutCheck;
    int                             _epollFd;
    
    Server(const Server& other);
    Server& operator=(const Server& other);

    void    acceptNewConnection(Socket* masterSocket);
    void    handleClientReceive(Client* client, epoll_event *event);  // EPOLLIN: istemciden veri alma
    void    handleClientSend(Client* client, epoll_event *event);     // EPOLLOUT: istemciye veri gönderme
    void    checkExpiredSockets();
    void    registerHandler(EpollHandler* socket);
    void    unregisterHandler(EpollHandler* socket);
    void    handleParsedRequest(Client* client, epoll_event* event, Client::StreamState state);
    void    reapCgiProcess(CgiHandler* handler);
    void    startCgi(Client* client, epoll_event* event, const std::string& scriptPath, const std::string& interpreterPath);
    void    handleCgiReceive(CgiHandler* cgiHandler);
    void    finishCgiResponse(CgiHandler* cgiHandler);

    
public:

    Server();
    ~Server();

    void    init(const ConfigParser& config); // Config dosyasını alıp socketleri (ip:port) açar
    void    run();

};

#endif
