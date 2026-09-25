#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

namespace StaticHandler
{
    HttpResponse get(const HttpRequest& request, const LocationConfig& location,
                     const ServerConfig& serverConfig);
}

#endif