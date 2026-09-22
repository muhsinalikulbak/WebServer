#ifndef ERRORRESPONSE_HPP
#define ERRORRESPONSE_HPP

#include "HttpResponse.hpp"

struct ServerConfig;
struct LocationConfig;

namespace ErrorResponse
{
    HttpResponse build(int statusCode, const ServerConfig& serverConfig);

    // status(): generic bir HTTP status cevabı üretir (hata sayfası DEĞİL, config'teki
    // error_page'e bakmaz). redirect() ve ResponseBuilder::handlePost gibi "ben zaten doğru
    // status kodunu ve opsiyonel Location header'ını biliyorum, sadece HTML gövdesiyle
    // sarmala" diyen çağıranlar için.
    HttpResponse status(int statusCode, const std::string& locationHeader = "");

    HttpResponse redirect(const LocationConfig& location);
}

#endif