#ifndef SERVER_HPP
#define SERVER_HPP
#include "Client.hpp"
#include <string>
#include <map>
#include <poll.h>
#include <vector>

class Server
{
private:
    // Client'in heap'de olmasının sebebi map kopyalama yaprken eski Client'ler stack de olduğundan hafızadan silinir ve SOCKET KAPANIR.
    std::map<int, Client*> _clientMap;
    std::vector<struct pollfd> _pollFds;
    
    Socket _masterSocket;

public:
    Server(/* args */);
    ~Server();
    
    void init(int port); // port numarası alır ve masterSocket'e verir  
    void run();
};


#endif // SERVER_HPP
