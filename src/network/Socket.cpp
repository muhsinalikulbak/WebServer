#include "Socket.hpp"
#include <exception>
#include <stdio.h>
#include <iostream>

Socket::Socket(/* args */)
{
    _fd = -1;
    _domain = -1;
    _type = -1;
    _protocol = -1;
    _state = (State)IDLE;
    _role = ROLE_IDLE;
    _readBuffer = "";
    _writeBuffer = "";
}

Socket::~Socket()
{
    closeSocket();
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
    return _fd;
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
    if (listen(_fd, SOMAXCONN) == -1)
    {
        perror("Listen");
        // Exception Fırlat;
    }
    _state = LISTENING;
}

// addr sockaddr_in olduğu için cast yapıyoruz.
int Socket::acceptConnection()
{
    sockaddr_in client_addr; // Client bilgilerini geçici olarak tutacak yer
    socklen_t len = sizeof(client_addr);
    
    // İşletim sistemi client bilgilerini sunucunun üzerine değil, client_addr'ye yazacak
    int client_fd = accept(_fd, (sockaddr*)&client_addr, &len);
    
    if (client_fd == -1) {
        perror("accept");
    }
    return client_fd;
}


void Socket::readFromClient(int clientFd)
{
    char buffer[4096];
    int byte = 1;
    std::string request;

    while (byte > 0)
    {
        byte = (int)recv(clientFd, buffer, 4096, 0);
        if (byte == -1)
        {
            perror("Recv");
            return; // Exception fırlat
        }
        else if (byte == 0)
        {
            // Client bağlantıyı kapattı.
            return;
        }
        request.append(buffer, byte);
        if (request.find("\r\n\r\n") != std::string::npos)
        {
            std::cout << "Request ready" << std::endl;
            break;
        }
    }
    std::cout << request << std::endl;
}

void Socket::closeSocket()
{
    if (_fd != -1)
    {
        close(_fd);
        _fd = -1;
        _state = IDLE;
    }
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
