#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include <map>
#include <string>

class ResponseBuilder
{
public:
    enum RouteResult
    {
        ROUTE_RESPOND_DIRECTLY,
        ROUTE_STATIC,
        ROUTE_CGI
    };

    static HttpResponse dispatch(const HttpRequest& request, const LocationConfig& location,
                                 const ServerConfig& serverConfig);
    static RouteResult routeRequest(const HttpRequest& request,
                                    const ServerConfig& serverConfig,
                                    HttpResponse& outErrorResponse,
                                    std::string& outScriptPath,
                                    std::string& outInterpreterPath,
                                    const LocationConfig*& outLocation);

    static HttpResponse buildErrorResponse(int statusCode, const ServerConfig& serverConfig);

    static std::string  resolveFilePath(const std::string& requestPath, const LocationConfig& location);
    
private:
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder& other);
    ResponseBuilder& operator=(const ResponseBuilder& other);
    ~ResponseBuilder();

    static bool         isMethodAllowedForLocation(const std::string& method, const LocationConfig& location);
    static bool         isCgiRequest(const std::string& path, const LocationConfig& location, std::string& outExtension);
};

#endif