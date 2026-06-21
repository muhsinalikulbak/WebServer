#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

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


public:
    Socket();
    Socket(int domain, int type);

    ~Socket();

    int createSocket();
    int acceptConnection();
    void startListening();
    void bindSocket(int port);

    
    void readFromClient(int clientFd); // Taşınacak

    int getFd();
    int getDomain();
    int getType();

};



#endif // SOCKET_HPP
