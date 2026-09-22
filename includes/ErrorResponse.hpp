#ifndef ERRORRESPONSE_HPP
#define ERRORRESPONSE_HPP

#include "HttpResponse.hpp"

struct ServerConfig;

namespace ErrorResponse
{
    HttpResponse build(int statusCode, const ServerConfig& serverConfig);
}

#endif