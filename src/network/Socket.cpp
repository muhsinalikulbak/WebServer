#include "Socket.hpp"

#include <exception>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>


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
  
  int opt = 1;
  if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    throw std::runtime_error(std::string("setsockopt SO_REUSEADDR: ") + strerror(errno));

  _state = CREATED;

  // GET_FLAG Kullanmamak büyük problem yaratacak mı bize
  if (fcntl(_fd, F_SETFL, O_NONBLOCK) == -1)
    throw std::runtime_error(std::string("fcntl F_SETFL failed: ") + strerror(errno));
}

void Socket::bindSocket(const std::string& host, int port)
{
  _addr.sin_family = _domain; // Genellikle AF_INET
  _addr.sin_port = htons(port); // Genellikle 8080, portu 80 Olarak deneyelim daha sonra.

    // config'te yazan IP'yi kullan
  if (host == "0.0.0.0" || host.empty())
      _addr.sin_addr.s_addr = INADDR_ANY;
  else
      _addr.sin_addr.s_addr = inet_addr(host.c_str());


  // INADDR_ANY = 0.0.0.0 sunucuya bağlı tüm ip'lerden atılan istekleri kabul et demektir.
  // Fiziksel ip, localhost ip, wifi ip gibi farklı ağ girişlerinden gelen
  // istekleri alır. Çünkü 0.0.0.0 = ANY demektir. İstersek bunu sınırlayabilir
  // sadece localhosttan ya da fiziksel ip'den istekleri kabul edebiliriz.

  if (bind(_fd, (sockaddr *)&_addr, sizeof(_addr)) == -1)
  {
    throw std::runtime_error(std::string("Socket bind failed: ") + strerror(errno));
  }
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

int Socket::acceptConnection()
{
  sockaddr_in client_addr; // Client bilgilerini geçici olarak tutacak yer
  socklen_t len = sizeof(client_addr);

  // İşletim sistemi client bilgilerini sunucunun üzerine değil, client_addr'ye
  // yazacak
  int clientFd = accept(_fd, (sockaddr *)&client_addr, &len);

  // Client adresi içine client bilgileri dolar, ip port vs
  if (clientFd == -1)
  {
    perror("Accept error"); 
  }
  // inet_ntoa (Network to ASCII): Sayısal IP'yi yazıya döker

  if (clientFd != -1)
  {
    std::cout << "Yeni baglanti: " << inet_ntoa(client_addr.sin_addr) << ":"
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

 