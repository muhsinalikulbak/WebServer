#include "RequestValidator.hpp"

bool RequestValidator::isMethodAllowed(const std::string& method)
{
    return (method == "get" || method == "post" || method == "delete");
}

bool RequestValidator::isUriValid(const std::string& uri)
{
    return (!uri.empty() && uri[0] == '/');
}

bool RequestValidator::isVersionSupported(const std::string& version)
{
    return (version == "http/1.1");
}

bool RequestValidator::hasRequiredHostHeader(const HttpRequest& request)
{
    return request.hasHeader("host");
}

int RequestValidator::validate(const HttpRequest& request)
{
    /*
    * Kontrol sırası ve dönen status kodları:
    * 1) Method whitelist kontrolü: desteklenmeyen method -> 405
    * 2) URI format kontrolü: boş ya da '/' ile başlamayan URI -> 400
    * 3) HTTP version kontrolü: http/1.1 dışı -> 505
    * 4) Host header zorunluluğu: eksik Host header -> 400
    * İlk başarısız kontrolün status kodu döner; tüm kontroller geçerse 0 döner.
    */
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
