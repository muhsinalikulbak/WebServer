#include "ResponseQueue.hpp"

#include <sys/epoll.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <stdexcept>

namespace ResponseQueue
{
    // Queues the response in the client's write buffer and registers the socket with epoll for EPOLLOUT; handles errors according to throwOnError.
    void push(int epollFd, Client* client, const HttpResponse& response, bool throwOnError)
    {
        client->setWriteBuffer(response.serialize());

        struct epoll_event event;
        std::memset(&event, 0, sizeof(event));
        event.data.ptr = client;
        event.events = EPOLLOUT;

        if (epoll_ctl(epollFd, EPOLL_CTL_MOD, client->getFd(), &event) == -1)
        {
            if (throwOnError)
                throw std::runtime_error(std::string("Error modifying to EPOLLOUT: ") + strerror(errno));
            else
                std::cerr << "Error modifying client to EPOLLOUT: " << strerror(errno) << std::endl;
        }
    }
}
