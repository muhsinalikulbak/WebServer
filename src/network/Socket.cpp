#include "Socket.hpp"

#include <exception>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <netdb.h>
#include <sstream>

Socket::Socket(int domain, int type)
{
  _fd = -1;
  _domain = domain;
  _type = type;
  _state = IDLE;
  _serverConfig = NULL;
}

Socket::Socket()
{
  _fd = -1;
  _domain = AF_INET;   // IPv4
  _type = SOCK_STREAM; // TCP
  _state = IDLE;
  _serverConfig = NULL;
}


Socket::~Socket()
{
  if (_fd != -1)
  {
    close(_fd);
  }
}

void Socket::createSocket()
{
  _fd = socket(_domain, _type, 0);

  if (_fd == -1)
  {
    throw std::runtime_error(std::string("Socket creation failed: ") + strerror(errno));
  }
  
  // SO_REUSEADDR: socket oluştuktan hemen sonra atılır.
  // Bind etmeden önce, server kapandıktan sonra portu hemen tekrar açabilmek için
  // kullanılır. Yoksa bind adress already uyarısı var ve TIME-WAIT atarak biraz bekletir.
  
  FdUtils::setReuseAddress(_fd);
  FdUtils::setNonBlocking(_fd);
  FdUtils::setCloseOnExec(_fd);
  
  
  _state = CREATED;
}


// INADDR_ANY = 0.0.0.0 sunucuya bağlı tüm ip'lerden atılan istekleri kabul et demektir.
// Fiziksel ip, localhost ip, wifi ip gibi farklı ağ girişlerinden gelen
// istekleri alır. Çünkü 0.0.0.0 = ANY demektir. İstersek bunu sınırlayabilir
// sadece localhosttan ya da fiziksel ip'den istekleri kabul edebiliriz.


void Socket::bindSocket(const std::string& host, int port)
{
    struct addrinfo hints;
    struct addrinfo* result = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = _domain;     // AF_INET
    hints.ai_socktype = _type;       // SOCK_STREAM
    hints.ai_flags    = AI_PASSIVE;  // node NULL olduğunda INADDR_ANY/wildcard ver

    std::ostringstream portStream;
    portStream << port;
    std::string portStr = portStream.str();

    // host boşsa veya 0.0.0.0 ise node'u NULL bırak -> AI_PASSIVE ile wildcard bind
    bool wildcard = (host.empty() || host == "0.0.0.0");
    const char* node = wildcard ? NULL : host.c_str();

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

void Socket::startListening()
{
  // Socket Maximum Connections, bir ağ sunucusunun (server)
  // dinleme sırasına alabileceği maksimum bekleyen (pending) bağlantı
  // talebi sayısını belirleyen sistem limitidir.

  if (listen(_fd, SOMAXCONN) == -1)
  {
    throw std::runtime_error(std::string("Socket listen failed: ") + strerror(errno));
  }
  _state = LISTENING;
}

// Yeni client ekleneceği zaman çağrılır

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


bool                Socket::isListening() const { return true; } // Override

int                 Socket::getFd() const { return _fd; } // Override

int                 Socket::getDomain() const { return _domain; }

int                 Socket::getType() const { return _type; }

State               Socket::getState() const { return _state; }

const ServerConfig* Socket::getServerConfig() const { return _serverConfig; }

void                Socket::setServerConfig(const ServerConfig* config) { _serverConfig = config; }

 