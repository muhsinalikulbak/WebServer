#ifndef SOCKET_HPP
#define SOCKET_HPP


#include "EpollHandler.hpp"

#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>


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

 
public:
    Socket();
    Socket(int domain, int type);
    ~Socket();


    virtual bool    isListening() const;

    int             acceptConnection();
    void            createSocket();
    void            startListening();
    void            bindSocket(int port);

    virtual int     getFd() const;
    int             getDomain() const;
    int             getType() const;
    State           getState() const;

};



#endif // SOCKET_HPP
