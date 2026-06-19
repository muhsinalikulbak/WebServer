#include <iostream>
#include "Socket.hpp"
#include <sys/socket.h>

int main()
{
    Socket sc;
    sc.createSocket();
    sc.bindSocket();
    sc.startListening();


    while (true)
    {
        int client = sc.acceptConnection();
        if (client != -1)
            std::cout << "New client: " << client << std::endl;
    }
}
