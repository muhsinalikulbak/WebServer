#include "Socket.hpp"

#include <exception>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <netdb.h>
#include <sstream>

// Creates a Socket in the IDLE state with the given host, port, and server configuration.
Socket::Socket(const std::string& host, int port, const ServerConfig& config) : _serverConfig(config)
{
  _fd = -1;

  _state = IDLE;
  _host = host;
  _port = port;
}

// Closes the socket file descriptor if it is open.
Socket::~Socket()
{
  if (_fd != -1)
  {
    close(_fd);
  }
}

// Creates a TCP socket and configures non-blocking and address reuse options.
void Socket::createSocket()
{
  _fd = socket(AF_INET, SOCK_STREAM, 0);

  if (_fd == -1)
  {
    throw std::runtime_error(std::string("Socket creation failed: ") + strerror(errno));
  }
  
  FdUtils::setReuseAddress(_fd);
  FdUtils::setReusePort(_fd);
  FdUtils::setNonBlocking(_fd);
  FdUtils::setCloseOnExec(_fd);
  
  
  _state = CREATED;
}

// Binds the socket to the host:port address resolved by getaddrinfo.
void Socket::bindSocket()
{
    struct addrinfo hints;
    struct addrinfo* result = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    std::ostringstream portStream;
    portStream << _port;
    std::string portStr = portStream.str();

    bool wildcard = (_host.empty() || _host == "0.0.0.0");
    const char* node = wildcard ? NULL : _host.c_str();

    int ret = getaddrinfo(node, portStr.c_str(), &hints, &result);
    if (ret != 0)
        throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(ret));

    if (bind(_fd, result->ai_addr, result->ai_addrlen) == -1)
    {
        freeaddrinfo(result);
        throw std::runtime_error(std::string("Socket bind failed: ") + strerror(errno));
    }

    freeaddrinfo(result);
    _state = BOUND;
}

// Puts the socket into listening mode with a SOMAXCONN backlog.
void Socket::startListening()
{
  if (listen(_fd, SOMAXCONN) == -1)
  {
    throw std::runtime_error(std::string("Socket listen failed: ") + strerror(errno));
  }
  _state = LISTENING;
}

// Accepts a pending connection and returns the new client file descriptor; logs connection details on success.
int Socket::acceptConnection()
{
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int clientFd = accept(_fd, (sockaddr *)&client_addr, &len);

    if (clientFd != -1)
    {
        uint32_t addr = ntohl(client_addr.sin_addr.s_addr);
        char ipStr[16];
        std::snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8)  & 0xFF,
            addr & 0xFF);

        std::cout << "Yeni baglanti: " << ipStr << ":"
                  << ntohs(client_addr.sin_port) << std::endl;
    }
    return clientFd;
}

// Returns this handler's epoll handler type (LISTEN).
EpollHandler::HandlerType Socket::getType() const { return EpollHandler::HANDLER_LISTEN; }

// Returns the socket file descriptor.
int                       Socket::getFd() const { return _fd; }

// Returns the bound host address.
const std::string&        Socket::getHost() const { return _host; }

// Returns the bound port.
int                       Socket::getPort() const { return _port; }

// Returns the socket's current state (IDLE/CREATED/BOUND/LISTENING, etc.).
Socket::State             Socket::getState() const { return _state; }

// Returns the server configuration associated with this socket.
const ServerConfig& Socket::getServerConfig() const { return _serverConfig; }