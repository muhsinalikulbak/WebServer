#include <iostream>
#include "Socket.hpp"
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    Socket sc;
    sc.createSocket();
    sc.bindSocket();
    sc.startListening();

    int client = 0;
    while (true)
    {
        client = sc.acceptConnection();
        if (client != -1)
            std::cout << "New client: " << client << std::endl;
        sc.readFromClient(client);
    }
    close(client);
}
