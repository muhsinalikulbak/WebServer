#ifndef RESPONSEQUEUE_HPP
#define RESPONSEQUEUE_HPP

#include "Client.hpp"
#include "HttpResponse.hpp"

namespace ResponseQueue
{
    void push(int epollFd, Client* client, const HttpResponse& response, bool throwOnError);
}

#endif
