#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>

enum State
{
    IDLE,
    CREATED,
    BOUND,
    LISTENING,
    CONNECTED,
    CLOSED
};

class Socket
{
private:
    int _fd;
    int _domain;   // AF_INET
    int _type;     // SOCK_STREAM
    struct sockaddr_in _addr;
    State _state;


public:
    Socket();
    Socket(int domain, int type);
    ~Socket();

    int acceptConnection();
    void createSocket();
    void startListening();
    void bindSocket(int port);

    
    int getFd() const;
    int getDomain() const;
    int getType() const;
    State getState() const;

};



#endif // SOCKET_HPP
