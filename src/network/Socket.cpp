#include "Socket.hpp"
#include <exception>

Socket::Socket(/* args */)
{
    _fd = -1;
    _domain = -1;
    _type = -1;
    _protocol = -1;
    _state = (State)IDLE;
    _role = IDLE;
    _readBuffer = "";
    _writeBuffer = "";
}

Socket::~Socket()
{

}

int Socket::createSocket()
{
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
    {
        perror("socket");
        return -1; // Exception fırlat
    }
    _state = CREATED;

}

void Socket::bindSocket()
{
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(8080);
    _addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_fd, (sockaddr*)&_addr, sizeof(_addr)) == -1)
    {
        perror("bind");
        // Exception Fırlat
    }
    _state = BOUND;
}

void Socket::startListening()
{
    if (listen(_fd, SOMAXCONN) == -1);
    {
        perror("Listen");
        // Exception Fırlat;
    }
    _state = LISTENING;
}

int Socket::acceptConnection()
{

}


void Socket::closeSocket()
{

}


int Socket::getFd()
{
    return _fd;
}

int Socket::getDomain()
{
    return _domain;
}

int Socket::getType()
{
    return _type;
}

int Socket::getProtocol()
{
    return  _protocol;
}
