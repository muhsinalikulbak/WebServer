#include "RequestValidator.hpp"

// HTTP metodunun sunucu tarafından genel olarak desteklenip desteklenmediğini kontrol eder.
bool RequestValidator::isMethodAllowed(const std::string& method)
{
    return (method == "get" || method == "post" || method == "delete");
}

// URI'nin boş olmadığını ve '/' ile başladığını kontrol eder.
bool RequestValidator::isUriValid(const std::string& uri)
{
    return (!uri.empty() && uri[0] == '/');
}

// HTTP versiyonunun desteklenen tek versiyon (HTTP/1.1) olup olmadığını kontrol eder.
bool RequestValidator::isVersionSupported(const std::string& version)
{
    return (version == "http/1.1");
}

// Zorunlu Host header'ının istekte bulunup bulunmadığını kontrol eder.
bool RequestValidator::hasRequiredHostHeader(const HttpRequest& request)
{
    return request.hasHeader("host");
}

// Method, URI, versiyon ve Host header sırasıyla doğrular; geçersizse uygun HTTP status kodunu döner.
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