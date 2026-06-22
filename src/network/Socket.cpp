#include "Socket.hpp"
#include <exception>
#include <stdio.h>
#include <iostream>

Socket::Socket(int domain, int type)
{
    _fd = -1;
    _domain = domain;
    _type = type;
    _state = IDLE;
}

Socket::Socket(/* args */)
{
    _fd = -1;
    _domain = AF_INET;
    _type = SOCK_STREAM;
    _state = IDLE;
}

Socket::~Socket()
{
    if (_fd != -1)
    {
        close(_fd);
        _state = CLOSED;
        _fd = -1;
    }
}

int Socket::createSocket()
{
    _fd = socket(_domain, _type, 0);
    if (_fd == -1)
    {
        perror("socket");
        return -1; // Exception fırlat
    }
    _state = CREATED;
    return _fd;
}

void Socket::bindSocket(int port)
{
    _addr.sin_family = _domain; // Genellikle AF_INET
    _addr.sin_port = htons(port); // Genellikle 8080
    _addr.sin_addr.s_addr = INADDR_ANY; // BU ne

    if (bind(_fd, (sockaddr*)&_addr, sizeof(_addr)) == -1)
    {
        perror("bind");
        // Exception Fırlat
    }
    _state = BOUND;
}

void Socket::startListening()
{
    // SOMAXCONN ?
    if (listen(_fd, SOMAXCONN) == -1)
    {
        perror("Listen");
        // Exception Fırlat;
    }
    _state = LISTENING;
}

int Socket::acceptConnection()
{
    sockaddr_in client_addr; // Client bilgilerini geçici olarak tutacak yer
    socklen_t len = sizeof(client_addr);
    
    // İşletim sistemi client bilgilerini sunucunun üzerine değil, client_addr'ye yazacak
    int client_fd = accept(_fd, (sockaddr*)&client_addr, &len);
    
    if (client_fd == -1) {
        perror("accept");
    }
    _state = CONNECTED;
    return client_fd;
}


int Socket::getFd() const
{
    return _fd;
}

int Socket::getDomain() const
{
    return _domain;
}

int Socket::getType() const
{
    return _type;
}


