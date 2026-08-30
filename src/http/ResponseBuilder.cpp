#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"

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
}


