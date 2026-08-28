#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include <string>

// Router ve RequestValidator gibi stateless bir utility class.
// Instance oluşturulmasına gerek yok, tüm metodlar static.
class ResponseBuilder
{
public:
    // Server'ın çağıracağı tek public giriş noktası.
    // İçeride sırasıyla RequestValidator::validate() ve Router::match() çağrılır,
    // sonucuna göre uygun dala (error / redirect / GET / POST / DELETE) dallanılır.
    static HttpResponse build(const HttpRequest& request, const ServerConfig& serverConfig);

private:
    // Stateless class - instance/copy engellensin
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder& other);
    ResponseBuilder& operator=(const ResponseBuilder& other);
    ~ResponseBuilder();

    // --- Method bazlı işlemler ---
    static HttpResponse handleGet(const HttpRequest& request, const LocationConfig& location);
    static HttpResponse handlePost(const HttpRequest& request, const LocationConfig& location, size_t clientMaxBodySize);
    static HttpResponse handleDelete(const HttpRequest& request, const LocationConfig& location);

    // --- Yönlendirme / kontrol yardımcıları ---
    static bool         isMethodAllowedForLocation(const std::string& method, const LocationConfig& location);
    static bool         isCgiRequest(const std::string& path, const LocationConfig& location, std::string& outExtension);
    static HttpResponse buildRedirect(const LocationConfig& location);

    // --- Dosya sistemi yardımcıları ---
    static std::string  resolveFilePath(const std::string& requestPath, const LocationConfig& location);
    static bool         pathExists(const std::string& path);
    static bool         isDirectory(const std::string& path);
    static bool         readFile(const std::string& path, std::string& outContent);
    static std::string  getContentType(const std::string& path);

    // --- Autoindex / error sayfaları ---
    static HttpResponse buildAutoindexPage(const std::string& dirPath, const std::string& requestPath);
    static HttpResponse buildErrorResponse(int statusCode, const ServerConfig& serverConfig);
};

#endif