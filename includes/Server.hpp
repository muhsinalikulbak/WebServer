#ifndef SERVER_HPP
#define SERVER_HPP
#include "Client.hpp"
#include <string>
#include <map>
#include <sys/epoll.h>
#include <vector>
#include <cstring>

class Server
{
private:
    // Client'in heap'de olmasının sebebi map kopyalama yaparken eski Client'ler stack de olduğundan hafızadan silinir ve SOCKET KAPANIR.
    std::map<int, Client*> _clientMap;
    Socket _masterSocket;
    
    int _epollFd;
    struct epoll_event _masterEvent;
    std::vector<struct epoll_event> _events; // epoll_wait'in dolduracağı dinamik dizi

public:
    Server(/* args */);
    ~Server();
    
    void init(int port); // port numarası alır ve masterSocket'e verir  
    void run();
    bool acceptNewConnection();
    void handleClientData(int clientFd);
};


#endif // SERVER_HPP
