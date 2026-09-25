#ifndef UPLOADHANDLER_HPP
#define UPLOADHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

namespace UploadHandler
{
    HttpResponse post(const HttpRequest& request, const LocationConfig& location,
                      const ServerConfig& serverConfig);
    HttpResponse remove(const HttpRequest& request, const LocationConfig& location,
                        const ServerConfig& serverConfig);
}

#endif