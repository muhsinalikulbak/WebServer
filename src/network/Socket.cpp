#include "Socket.hpp"

#include <exception>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <netdb.h>
#include <sstream>

// Verilen host/port ve server config ile IDLE durumunda bir Socket oluşturur.
Socket::Socket(const std::string& host, int port, const ServerConfig& config) : _serverConfig(config)
{
  _fd = -1;

  _state = IDLE;
  _host = host;
  _port = port;
}

// Socket fd'si açıksa kapatır.
Socket::~Socket()
{
  if (_fd != -1)
  {
    close(_fd);
  }
}

// TCP soketini oluşturup non-blocking/reuse seçeneklerini ayarlar.
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

// Soketi getaddrinfo ile çözümlenen host:port adresine bind eder.
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

// Soketi SOMAXCONN bekleme kuyruğuyla dinleme moduna alır.
void Socket::startListening()
{
  if (listen(_fd, SOMAXCONN) == -1)
  {
    throw std::runtime_error(std::string("Socket listen failed: ") + strerror(errno));
  }
  _state = LISTENING;
}

// Bekleyen bir bağlantıyı kabul edip yeni client fd'sini döner; kabul edilirse bağlantı bilgisini loglar.
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

// Bu handler'ın epoll handler tipini (LISTEN) döner.
EpollHandler::HandlerType Socket::getType() const { return EpollHandler::HANDLER_LISTEN; }

// Soketin fd'sini döner.
int                       Socket::getFd() const { return _fd; }

// Bağlı olduğu host adresini döner.
const std::string&        Socket::getHost() const { return _host; }

// Bağlı olduğu portu döner.
int                       Socket::getPort() const { return _port; }

// Soketin mevcut durumunu (IDLE/CREATED/BOUND/LISTENING vb.) döner.
Socket::State             Socket::getState() const { return _state; }

// Bu soketin bağlı olduğu server config'ini döner.
const ServerConfig& Socket::getServerConfig() const { return _serverConfig; }