#include "RequestValidator.hpp"

// Checks whether the HTTP method is generally supported by the server.
bool RequestValidator::isMethodAllowed(const std::string& method)
{
    return (method == "get" || method == "post" || method == "delete");
}

// Checks that the URI is non-empty and starts with '/'.
bool RequestValidator::isUriValid(const std::string& uri)
{
    return (!uri.empty() && uri[0] == '/');
}

// Checks whether the HTTP version is the only supported version, HTTP/1.1.
bool RequestValidator::isVersionSupported(const std::string& version)
{
    return (version == "http/1.1");
}

// Checks whether the required Host header is present in the request.
bool RequestValidator::hasRequiredHostHeader(const HttpRequest& request)
{
    return request.hasHeader("host");
}

// Validates the method, URI, version, and Host header in order; returns the appropriate HTTP status code if invalid.
int RequestValidator::validate(const HttpRequest& request)
{
    if (!isMethodAllowed(request.getMethod()))
        return 405;
    if (!isUriValid(request.getUri()))
        return 400;
    if (!isVersionSupported(request.getVersion()))
        return 505;
    if (!hasRequiredHostHeader(request))
        return 400;
    return 0;
}