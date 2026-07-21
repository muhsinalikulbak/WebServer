#include "Socket.hpp"
#include <exception>
#include <iostream>
#include <stdio.h>

Socket::Socket(int domain, int type)
{
  _fd = -1;
  _domain = domain;
  _type = type;
  _state = IDLE;
}

Socket::Socket()
{
  _fd = -1;
  _domain = AF_INET;   // IPv4
  _type = SOCK_STREAM; // TCP
  _state = IDLE;
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

  // SO_REUSEADDR: socket oluştuktan hemen sonra atılır.
  // Bind etmeden önce, server kapandıktan sonra portu hemen tekrar açmak için
  // kullanılır.
  int opt = 1;
  if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    perror("setsockopt SO_REUSEADDR");

  if (_fd == -1)
  {
    perror("socket");
    throw std::runtime_error("Socket creation failed");
  } 
  else
    _state = CREATED;

  int flags = fcntl(_fd, F_GETFL, 0);
  fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
}

void Socket::bindSocket(int port)
{
  _addr.sin_family = _domain; // Genellikle AF_INET
  _addr.sin_port = htons(port); // Genellikle 8080, portu 80 Olarak deneyelim daha sonra.

  _addr.sin_addr.s_addr = INADDR_ANY; 
  // Bu sunucuya bağlı tüm ip'lerden atılan istekleri kabul et demektir.
  // Fiziksel ip, localhost ip, wifi ip gibi farklı ağ girişlerinden gelen
  // istekleri alır. Çünkü 0.0.0.0 = ANY demektir. İstersek bunu sınırlayabilir
  // sadece localhosttan ya da fiziksel ip'den istekleri kabul edebiliriz.

  if (bind(_fd, (sockaddr *)&_addr, sizeof(_addr)) == -1)
  {
    perror("bind");
    // Exception Fırlat
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
    perror("Listen");
    // Exception Fırlat;
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
    perror("accept");
  }
  // inet_ntoa (Network to ASCII): Sayısal IP'yi yazıya döker

  if (clientFd != -1)
  {
    std::cout << "Yeni baglanti: " << inet_ntoa(client_addr.sin_addr) << ":"
              << ntohs(client_addr.sin_port) << std::endl;
  }
  return clientFd;
}

int Socket::getFd() const { return _fd; }

int Socket::getDomain() const { return _domain; }

int Socket::getType() const { return _type; }

State Socket::getState() const { return _state; }
