#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

// Statik dosya servisinin tek karar noktası.
// routeRequest ROUTE_STATIC döndüğünde dispatch() bu handler'ı çağırır.
namespace StaticHandler
{
    HttpResponse get(const HttpRequest& request, const LocationConfig& location,
                     const ServerConfig& serverConfig);
}

#endif