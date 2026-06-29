#include "Server.hpp"
#include "Socket.hpp"

Server::Server(/* args */)
{

}

Server::~Server()
{
    std::map <int, Client*>::iterator it = _clientMap.begin();
    
    while (it != _clientMap.end())
    {
        delete it->second;
        it++;
    }
}

void Server::init(int port)
{
    _masterSocket.createSocket();
    _masterSocket.bindSocket(port);
    _masterSocket.startListening();

    struct pollfd masterPollFd;
    masterPollFd.events = POLLIN; // Yeni veri ya da bağlantı gelirse uyar.
    masterPollFd.fd = _masterSocket.getFd();
    masterPollFd.revents = 0;

    _pollFds.push_back(masterPollFd);

    run();
}

void Server::run()
{
    while (true)
    {
        int pollResult = poll(&_pollFds[0], _pollFds.size(), -1);

        if (pollResult == -1)
        {
            perror("Poll error");
            // Throw fırlat
        }

        for (size_t i = 0; i < _pollFds.size(); i++)
        {
            if (_pollFds[i].revents == POLLIN)
            {
                if (_pollFds[i].fd == _masterSocket.getFd())
                {
                    int clientFd = _masterSocket.acceptConnection();

                    if (clientFd != -1)
                    {
                        struct pollfd clientPollFd;

                        clientPollFd.events = POLLIN;
                        clientPollFd.revents = 0;
                        clientPollFd.fd = clientFd;

                        _pollFds.push_back(clientPollFd);

                        _clientMap[clientFd] = new Client(clientFd);

                    }
                }
                else
                {
                    _clientMap[_pollFds[i].fd]->readData();
                }
            }
        }
        
    }
}
