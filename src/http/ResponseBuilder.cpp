#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"

// sonucuna göre uygun dala (error / redirect / GET / POST / DELETE) dallanılır.
HttpResponse ResponseBuilder::build(const HttpRequest& request, const ServerConfig& serverConfig)
{
    HttpResponse response;

    response.setStatus(RequestValidator::validate(request));
    
    
}

HttpResponse ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
    HttpResponse response;
}


