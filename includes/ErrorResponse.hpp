#ifndef ERRORRESPONSE_HPP
#define ERRORRESPONSE_HPP

#include "HttpResponse.hpp"

struct ServerConfig;
struct LocationConfig;

namespace ErrorResponse
{
    HttpResponse build(int statusCode, const ServerConfig& serverConfig);
    HttpResponse redirect(const LocationConfig& location);
}

#endif