#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include <map>
#include <string>

// Router ve RequestValidator gibi stateless bir utility class.
// Instance oluşturulmasına gerek yok, tüm metodlar static.
class ResponseBuilder
{
public:
    enum RouteResult
    {
        ROUTE_RESPOND_DIRECTLY,
        ROUTE_STATIC,
        ROUTE_CGI
    };

    // Server'ın çağıracağı tek public giriş noktası.
    // İçeride sırasıyla RequestValidator::validate() ve Router::match() çağrılır,
    // sonucuna göre uygun dala (error / redirect / GET / POST / DELETE) dallanılır.
    static HttpResponse dispatch(const HttpRequest& request, const LocationConfig& location,
                                 const ServerConfig& serverConfig);
    static RouteResult routeRequest(const HttpRequest& request,
                                    const ServerConfig& serverConfig,
                                    HttpResponse& outErrorResponse,
                                    std::string& outScriptPath,
                                    std::string& outInterpreterPath,
                                    const LocationConfig*& outLocation);

    static HttpResponse buildErrorResponse(int statusCode, const ServerConfig& serverConfig);

    // Statik ve CGI path çözümlemesinde ortak kullanılan tek yardımcı;
    // StaticHandler da (handleGet) aynı çözümlemeyi kullandığından public'tir.
    static std::string  resolveFilePath(const std::string& requestPath, const LocationConfig& location);
    
private:
    // Stateless class - instance/copy engellensin
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder& other);
    ResponseBuilder& operator=(const ResponseBuilder& other);
    ~ResponseBuilder();

    // --- Yönlendirme / kontrol yardımcıları ---
    static bool         isMethodAllowedForLocation(const std::string& method, const LocationConfig& location);
    static bool         isCgiRequest(const std::string& path, const LocationConfig& location, std::string& outExtension);
};

#endif