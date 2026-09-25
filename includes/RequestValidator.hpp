#ifndef REQUESTVALIDATOR_HPP
#define REQUESTVALIDATOR_HPP

#include <string>

#include "HttpRequest.hpp"

class RequestValidator
{
private:
    static bool isMethodAllowed(const std::string& method);

    static bool isUriValid(const std::string& uri);

    static bool isVersionSupported(const std::string& version);

    static bool hasRequiredHostHeader(const HttpRequest& request);

public:
    static int validate(const HttpRequest& request);
};

#endif
