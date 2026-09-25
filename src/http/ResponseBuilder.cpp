#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"
#include "FileUtils.hpp"
#include "MimeTypes.hpp"
#include "ErrorResponse.hpp"
#include "HttpStatusResponse.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <sstream> 
#include <sys/stat.h>
#include <dirent.h>

// Validates the request, finds the matching location, selects an error, redirect, CGI, or static response, and returns the result.
ResponseBuilder::RouteResult ResponseBuilder::routeRequest(
    const HttpRequest& request,
    const ServerConfig& serverConfig,
    HttpResponse& outErrorResponse,
    std::string& outScriptPath,
    std::string& outInterpreterPath,
    const LocationConfig*& outLocation)
{
    outScriptPath.clear();
    outInterpreterPath.clear();

    int validationCode = RequestValidator::validate(request);
    if (validationCode)
    {
        outErrorResponse = buildErrorResponse(validationCode, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    const LocationConfig* location = Router::match(request.getPath(), serverConfig);
    outLocation = location;
    if (!location)
    {
        outErrorResponse = buildErrorResponse(404, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (request.getBody().size() > serverConfig.effectiveBodyLimit(location))
    {
        outErrorResponse = buildErrorResponse(413, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (location->returnCode != 0)
    {
        outErrorResponse = HttpStatusResponse::redirect(*location);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (!isMethodAllowedForLocation(request.getMethod(), *location))
    {
        outErrorResponse = buildErrorResponse(405, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    std::string cgiExt;
    if (!isCgiRequest(request.getPath(), *location, cgiExt))
        return ROUTE_STATIC;

    std::string scriptPath = resolveFilePath(request.getPath(), *location);
    if (scriptPath.empty())
    {
        outErrorResponse = buildErrorResponse(403, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (FileUtils::isDirectory(scriptPath))
    {
        outErrorResponse = buildErrorResponse(404, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    std::map<std::string, std::string>::const_iterator it =
        location->cgiExtension.find(cgiExt);
    if (it == location->cgiExtension.end())
    {
        outErrorResponse = buildErrorResponse(500, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    outScriptPath = scriptPath;
    outInterpreterPath = it->second;
    return ROUTE_CGI;
}

// Routes the request to StaticHandler or UploadHandler based on the method (called for ROUTE_STATIC).
HttpResponse ResponseBuilder::dispatch(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    if (request.getMethod() == "get")    return StaticHandler::get(request, location, serverConfig);
    if (request.getMethod() == "post")   return UploadHandler::post(request, location, serverConfig);
    if (request.getMethod() == "delete") return UploadHandler::remove(request, location, serverConfig);
    return buildErrorResponse(501, serverConfig);
}

// Builds an error response for the given status code through the ErrorResponse module.
HttpResponse ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
    return ErrorResponse::build(statusCode, serverConfig);
}

// Checks case-insensitively whether the method is in the list allowed by the location.
bool    ResponseBuilder::isMethodAllowedForLocation(const std::string& method, const LocationConfig& location)
{
    for (size_t i = 0; i < location.allowedMethods.size(); i++)
    {
        if (HttpRequest::toLowerCopy(location.allowedMethods[i]) == method)
            return true;
    }
    return false;
}

// Checks whether the path extension matches a CGI extension configured in the location.
bool ResponseBuilder::isCgiRequest(const std::string& path, const LocationConfig& location, std::string& outExtension)
{
    outExtension.clear();

    size_t slashPos = path.find_last_of('/');
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos)
        return false;
    if (slashPos != std::string::npos && dotPos < slashPos)
        return false;
    if (dotPos + 1 >= path.length())
        return false;

    outExtension = path.substr(dotPos);
    return (location.cgiExtension.find(outExtension) != location.cgiExtension.end());
}
    
// Maps the request path to a disk path by removing the location prefix and joining it to the root; rejects paths containing "..".
std::string ResponseBuilder::resolveFilePath(const std::string& requestPath, const LocationConfig& location)
{
    std::string remainder = requestPath.substr(location.path.length());

    if (remainder.find("..") != std::string::npos)
        return "";

    return FileUtils::joinPath(location.root, remainder);
}

