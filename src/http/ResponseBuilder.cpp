#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream> 


// sonucuna göre uygun dala (error / redirect / GET / POST / DELETE) dallanılır.
HttpResponse ResponseBuilder::build(const HttpRequest& request, const ServerConfig& serverConfig)
{
    int validationCode = RequestValidator::validate(request);

    if (validationCode)
        return buildErrorResponse(validationCode, serverConfig);

    const LocationConfig* location = Router::match(request.getPath(), serverConfig);
    
    if (!location)
        return buildErrorResponse(404, serverConfig);

    // return direktifi metoddan bağımsız çalışır (nginx semantiği) -> method check'ten önce
    if (location->returnCode != 0)
        return buildRedirect(*location);

    if (!isMethodAllowedForLocation(request.getMethod(), *location))
        return buildErrorResponse(405, serverConfig);

    std::string cgiExt;
    if (isCgiRequest(request.getPath(), *location, cgiExt))
        return buildErrorResponse(501, serverConfig); // CGI fazı henüz yok

    if (request.getMethod() == "get" || request.getMethod() == "head")
        return handleGet(request, *location);
    else if (request.getMethod() == "post")
        return handlePost(request, *location, serverConfig.clientMaxBodySize);
    else if (request.getMethod() == "delete")
        return handleDelete(request, *location);

    return buildErrorResponse(501, serverConfig); // buraya normalde düşmemeli
}

HttpResponse ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
    HttpResponse response;
    response.setStatus(statusCode);

    std::map<int, std::string>::const_iterator it = serverConfig.errorPages.find(statusCode);
    std::string body;
    bool loaded = false;

    if (it != serverConfig.errorPages.end())
        loaded = readFile(it->second, body);   // config'teki path'i doğrudan dene

    if (!loaded)
    {
        std::ostringstream html;
        html << "<html><head><title>" << statusCode << "</title></head><body>"
             << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
             << "</body></html>";
        body = html.str();
    }

    response.setHeader("Content-Type", "text/html");
    response.setBody(body);
    return response;
}

bool ResponseBuilder::readFile(const std::string& path, std::string& outContent)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    outContent = ss.str();
    return true;
}


bool    ResponseBuilder::isMethodAllowedForLocation(const std::string& method, const LocationConfig& location)
{
    for (size_t i = 0; i < location.allowedMethods.size(); i++)
    {
        if (location.allowedMethods[i] == method)
            return true;
    }
    return false;
}
