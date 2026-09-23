#ifndef UPLOADHANDLER_HPP
#define UPLOADHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

// Upload (POST) ve silme (DELETE) işlemlerinin tek karar noktası.
// routeRequest ROUTE_STATIC döndüğünde dispatch() uygun handler'ı çağırır.
namespace UploadHandler
{
    HttpResponse post(const HttpRequest& request, const LocationConfig& location,
                      const ServerConfig& serverConfig);
    HttpResponse remove(const HttpRequest& request, const LocationConfig& location,
                        const ServerConfig& serverConfig);
}

#endif