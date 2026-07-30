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


enum State
{
    IDLE,
    CREATED,
    BOUND,
    LISTENING,
    CONNECTED,
    CLOSED
};

class Socket : public EpollHandler
{
private:
    int                 _fd;
    int                 _domain;   // AF_INET
    int                 _type;     // SOCK_STREAM
    State               _state;
    struct sockaddr_in  _addr;
    const ServerConfig* _serverConfig; // bu socket hangi server bloğuna ait

    Socket(const Socket& other);
    Socket& operator=(const Socket& other);

public:
    Socket();
    Socket(int domain, int type);
    ~Socket();


    virtual bool        isListening() const;

    int                 acceptConnection();
    void                createSocket();
    void                startListening();
    void                bindSocket(const std::string& host, int port);

    virtual int         getFd() const;
    int                 getDomain() const;
    int                 getType() const;
    State               getState() const;
    const ServerConfig* getServerConfig() const;
    void                setServerConfig(const ServerConfig* config);

};



#endif // SOCKET_HPP
