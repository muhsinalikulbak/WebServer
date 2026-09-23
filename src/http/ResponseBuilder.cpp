#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"
#include "FileUtils.hpp"
#include "MimeTypes.hpp"
#include "ErrorResponse.hpp"
#include "HttpStatusResponse.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <sstream> 
#include <sys/stat.h>
#include <dirent.h>   // opendir/readdir/closedir için

ResponseBuilder::RouteResult ResponseBuilder::routeRequest(
    const HttpRequest& request,
    const ServerConfig& serverConfig,
    HttpResponse& outErrorResponse,
    std::string& outScriptPath,
    std::string& outInterpreterPath,
    const LocationConfig*& outLocation)
{
    outScriptPath.clear();
    outInterpreterPath.clear();

    int validationCode = RequestValidator::validate(request);
    if (validationCode)
    {
        outErrorResponse = buildErrorResponse(validationCode, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    const LocationConfig* location = Router::match(request.getPath(), serverConfig);
    // Eşleşen location dispatch aşamasında da gerektiği için dışarı verilir;
    // NULL olsa bile 404 dalına girmeden önce set edilir.
    outLocation = location;
    if (!location)
    {
        outErrorResponse = buildErrorResponse(404, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    // Parser sadece tavanı uygular; location'a özel gerçek limit route belli olduktan
    // sonra burada uygulanır. Redirect/method kontrolünden önce olması, fazla büyük body'nin
    // hangi handler'a gideceğinden bağımsız reddedilmesi içindir.
    if (request.getBody().size() > serverConfig.effectiveBodyLimit(location))
    {
        outErrorResponse = buildErrorResponse(413, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (location->returnCode != 0)
    {
        outErrorResponse = HttpStatusResponse::redirect(*location);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (!isMethodAllowedForLocation(request.getMethod(), *location))
    {
        outErrorResponse = buildErrorResponse(405, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    std::string cgiExt;
    if (!isCgiRequest(request.getPath(), *location, cgiExt))
        return ROUTE_STATIC;

    std::string scriptPath = resolveFilePath(request.getPath(), *location);
    if (scriptPath.empty())
    {
        outErrorResponse = buildErrorResponse(403, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    if (!FileUtils::pathExists(scriptPath) || FileUtils::isDirectory(scriptPath))
    {
        outErrorResponse = buildErrorResponse(404, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    std::map<std::string, std::string>::const_iterator it =
        location->cgiExtension.find(cgiExt);
    if (it == location->cgiExtension.end())
    {
        outErrorResponse = buildErrorResponse(500, serverConfig);
        return ROUTE_RESPOND_DIRECTLY;
    }

    outScriptPath = scriptPath;
    outInterpreterPath = it->second;
    return ROUTE_CGI;
}

// dispatch yalnızca ROUTE_STATIC durumunda çağrılır. routeRequest zaten
// method/redirect/413/404/get kontrolünü yapıp isteği doğrulamıştır; burada
// validate/match TEKRARLANMAZ. Eski build() bu kontrolleri routeRequest'ten
// bağımsız ikinci kez yapıyordu ve 413 kontrolü eksikti (routeRequest'te vardı) -
// iki fonksiyon sessizce sapmıştı. dispatch tek karar noktası olan routeRequest'e
// güvenerek bu riski ortadan kaldırır.
HttpResponse ResponseBuilder::dispatch(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    if (request.getMethod() == "get")    return StaticHandler::get(request, location, serverConfig);
    if (request.getMethod() == "post")   return UploadHandler::post(request, location, serverConfig);
    if (request.getMethod() == "delete") return UploadHandler::remove(request, location, serverConfig);
    return buildErrorResponse(501, serverConfig);   // teorik olarak ulaşılamaz güvenlik ağı
}

// Gerçek üretim ErrorResponse modülüne taşındı; davranış birebir korunur.
HttpResponse ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
    return ErrorResponse::build(statusCode, serverConfig);
}


bool    ResponseBuilder::isMethodAllowedForLocation(const std::string& method, const LocationConfig& location)
{
    // Location bazlı method kısıtını uygulamak için whitelist kontrolü yapar.
    // Config'ten gelen değerleri lower-case karşılaştırmak, yazım farklarından
    // doğacak yanlış negatifleri engelleyerek daha kararlı bir eşleştirme sağlar.
    // Buradaki toLowerCopy'e test ederken bir bak

    for (size_t i = 0; i < location.allowedMethods.size(); i++)
    {
        if (HttpRequest::toLowerCopy(location.allowedMethods[i]) == method)
            return true;
    }
    return false;
}

bool ResponseBuilder::isCgiRequest(const std::string& path, const LocationConfig& location, std::string& outExtension)
{
    outExtension.clear();

    size_t slashPos = path.find_last_of('/');
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos)
        return false;
    if (slashPos != std::string::npos && dotPos < slashPos)
        return false;
    if (dotPos + 1 >= path.length())
        return false;

    outExtension = path.substr(dotPos);
    return (location.cgiExtension.find(outExtension) != location.cgiExtension.end());
}
    
// requestPath (örn "/images/cat.png") ile matched location prefix'ini (location.path) çıkarıp
// kalanı location.root ile birleştirir, gerçek disk path'ini üretir.
// örn: location.path="/images", location.root="/var/www/static", requestPath="/images/cat.png"
//      -> kalan="/cat.png" -> sonuç="/var/www/static/cat.png/"
// GÜVENLİK: ".." içeren path'ler reddedilir (path traversal koruması). Geçersizse "" döner.

std::string ResponseBuilder::resolveFilePath(const std::string& requestPath, const LocationConfig& location)
{
    // URL path'ini filesystem path'ine çeviren temel çözümleyicidir.
    // Eşleşen location prefix'i atılır ve kalan bölüm root ile birleştirilir.
    // Böylece routing seviyesi ile disk yerleşimi birbirinden ayrıştırılır.
    std::string remainder = requestPath.substr(location.path.length());

    // ".." tespiti, üst dizinlere kaçış denemesini engellemek içindir.
    // Güvenlik ihlali riski olduğunda boş path döndürülerek üst katmanda 403 üretilir.
    if (remainder.find("..") != std::string::npos)
        return "";

    // root ve remainder arasındaki slash normalizasyonu FileUtils::joinPath'te ortaklaşır.
    return FileUtils::joinPath(location.root, remainder);
}

