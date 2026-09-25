#ifndef HTTPSTATUSRESPONSE_HPP
#define HTTPSTATUSRESPONSE_HPP

#include "HttpResponse.hpp"

struct LocationConfig;

namespace HttpStatusResponse
{
    std::string html(int statusCode);

    HttpResponse build(int statusCode, const std::string& locationHeader = "");

    HttpResponse redirect(const LocationConfig& location);
}

#endif