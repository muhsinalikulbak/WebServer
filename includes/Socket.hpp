#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
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
enum Role
{
    IDLE,
    SERVER,
    CLIENT
};

class Socket
{
private:
    int _fd;
    int _domain;   // AF_INET
    int _type;     // SOCK_STREAM
    int _protocol; // 0
    State _state;
    Role _role;
    std::string _readBuffer;
    std::string _writeBuffer;
    struct sockaddr_in _addr;


public:
    Socket(/* args */);
    ~Socket();

    int createSocket();
    void bindSocket();
    void startListening();
    int acceptConnection();
    void closeSocket();

    int getFd();
    int getDomain();
    int getType();
    int getProtocol();

};



#endif // SOCKET_HPP
