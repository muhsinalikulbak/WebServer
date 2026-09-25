#include "HttpStatusResponse.hpp"
#include "LocationConfig.hpp"
#include <sstream>

// Builds the shared status HTML body ("<title>CODE</title>" + "<h1>CODE DESCRIPTION</h1>").
std::string HttpStatusResponse::html(int statusCode)
{
    std::ostringstream ss;
    ss << "<html><head><title>" << statusCode << "</title></head><body>"
       << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
       << "</body></html>";
    return ss.str();
}

// Creates a generic HTTP response with the given status code and optional Location header.
HttpResponse HttpStatusResponse::build(int statusCode, const std::string& locationHeader)
{
    HttpResponse response;
    std::string body = html(statusCode);

    response.setStatus(statusCode);
    if (!locationHeader.empty())
        response.setHeader("Location", locationHeader);

    response.setHeader("Content-Type", "text/html");
    response.setBody(body);
    return response;
}

// Converts the return rule in the location configuration into an HTTP redirect response.
HttpResponse HttpStatusResponse::redirect(const LocationConfig& location)
{
    return build(location.returnCode, location.returnUrl);
}