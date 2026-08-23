#ifndef REQUESTVALIDATOR_HPP
#define REQUESTVALIDATOR_HPP

#include <string>

#include "HttpRequest.hpp"

class RequestValidator
{
private:
    // HTTP method sadece server tarafında genel olarak desteklenen bir yöntem mi diye bakar.
    static bool isMethodAllowed(const std::string& method);

    // URI boş olmamalı ve mutlak path gibi '/' ile başlamalı; aksi halde request anlamsal olarak geçersizdir.
    static bool isUriValid(const std::string& uri);

    // Bu sunucu şimdilik sadece HTTP/1.1 konuşur; HTTP/1.0 desteği ve diğer versiyonlar yoktur.
    static bool isVersionSupported(const std::string& version);

    // HTTP/1.1'de Host header zorunludur (RFC 7230 §5.4); sanal host seçimi için gerekli temel bilgidir.
    static bool hasRequiredHostHeader(const HttpRequest& request);

public:
    // Request geçerliyse 0 döner; değilse uygun HTTP status kodunu döner.
    static int validate(const HttpRequest& request);
};

#endif
