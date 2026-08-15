#ifndef SOCKET_HPP
#define SOCKET_HPP


#include "EpollHandler.hpp"
#include "ServerConfig.hpp"
#include "FdUtils.hpp"

#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/tcp.h>


class Socket : public EpollHandler
{
public:
    enum State
    {
        IDLE,
        CREATED,
        BOUND,
        LISTENING,
        CONNECTED,
        CLOSED
    };

private:
    int                 _fd;
    State               _state;
    struct sockaddr_in  _addr;
    std::string         _host;
    int                 _port;
    const ServerConfig& _serverConfig; // bu socket hangi server bloğuna ait

    Socket(const Socket& other);
    Socket& operator=(const Socket& other);

public:
    Socket();
    Socket(const std::string& host, int port, const ServerConfig& config);
    ~Socket();

    virtual EpollHandler::HandlerType getType() const;

    int                 acceptConnection();
    void                createSocket();
    void                startListening();
    void                bindSocket();

    virtual int         getFd() const;
    const std::string&  getHost() const;
    int                 getPort() const;
    State               getState() const;
    const ServerConfig& getServerConfig() const;
};



#endif // SOCKET_HPP
