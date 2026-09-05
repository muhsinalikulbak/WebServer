#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream> 
#include <sys/stat.h>
#include <dirent.h>   // opendir/readdir/closedir için

// sonucuna göre uygun dala (error / redirect / GET / POST / DELETE) dallanılır.
HttpResponse ResponseBuilder::build(const HttpRequest& request, const ServerConfig& serverConfig)
{
    int validationCode = RequestValidator::validate(request);

    if (validationCode)
        return buildErrorResponse(validationCode, serverConfig);

    const LocationConfig* location = Router::match(request.getPath(), serverConfig);
    
    if (!location)
        return buildErrorResponse(404, serverConfig);

    // return direktifi metoddan bağımsız çalışır (nginx semantiği) -> method check'ten önce
    if (location->returnCode != 0)
        return buildRedirect(*location);

    if (!isMethodAllowedForLocation(request.getMethod(), *location))
        return buildErrorResponse(405, serverConfig);

    std::string cgiExt;
    if (isCgiRequest(request.getPath(), *location, cgiExt))
        return buildErrorResponse(501, serverConfig); // CGI fazı henüz yok

    if (request.getMethod() == "get" || request.getMethod() == "head")
        return handleGet(request, *location, serverConfig);
    else if (request.getMethod() == "post")
        return handlePost(request, *location, serverConfig);
    else if (request.getMethod() == "delete")
        return handleDelete(request, *location, serverConfig);

    return buildErrorResponse(501, serverConfig);
}

HttpResponse ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
    HttpResponse response;
    response.setStatus(statusCode);

    std::map<int, std::string>::const_iterator it = serverConfig.errorPages.find(statusCode);
    std::string body;
    bool loaded = false;

    if (it != serverConfig.errorPages.end())
        loaded = readFile(it->second, body);   // config'teki path'i doğrudan dene

    if (!loaded)
    {
        std::ostringstream html;
        html << "<html><head><title>" << statusCode << "</title></head><body>"
             << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
             << "</body></html>";
        body = html.str();
    }

    response.setHeader("Content-Type", "text/html");
    response.setBody(body);
    return response;
}

bool ResponseBuilder::readFile(const std::string& path, std::string& outContent)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    outContent = ss.str();
    return true;
}


bool    ResponseBuilder::isMethodAllowedForLocation(const std::string& method, const LocationConfig& location)
{
    for (size_t i = 0; i < location.allowedMethods.size(); i++)
    {
        if (location.allowedMethods[i] == method)
            return true;
    }
    return false;
}

// requestPath (örn "/images/cat.png") ile matched location prefix'ini (location.path) çıkarıp
// kalanı location.root ile birleştirir, gerçek disk path'ini üretir.
// örn: location.path="/images", location.root="/var/www/static", requestPath="/images/cat.png"
//      -> kalan="/cat.png" -> sonuç="/var/www/static/cat.png"
// GÜVENLİK: ".." içeren path'ler reddedilir (path traversal koruması). Geçersizse "" döner.

std::string ResponseBuilder::resolveFilePath(const std::string& requestPath, const LocationConfig& location)
{
    std::string remainder = requestPath.substr(location.path.length());

    if (remainder.find("..") != std::string::npos)
        return "";

    std::string root = location.root;
    bool rootEndsSlash = !root.empty() && root[root.length() - 1] == '/';
    bool remainderStartsSlash = !remainder.empty() && remainder[0] == '/';

    if (rootEndsSlash && remainderStartsSlash)
        root.erase(root.length() - 1);          // çift slash -> tekine indir
    else if (!rootEndsSlash && !remainderStartsSlash)
        root += "/";                             // slash yok -> ekle

    return root + remainder;
}

// stat() ile path'in diskte var olup olmadığını kontrol eder (dosya ya da dizin fark etmez).
bool ResponseBuilder::pathExists(const std::string& path)
{
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

// stat() sonucundaki st_mode alanını S_ISDIR makrosuyla kontrol ederek path'in
// dizin mi olduğunu söyler. stat başarısızsa (yok/erişilemiyor) false döner.
bool ResponseBuilder::isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

// Path'in extension'ına bakıp uygun MIME type döner.
// Bilinmeyen extension -> "application/octet-stream" (browser bunu indirir, bozuk render etmez).
std::string ResponseBuilder::getContentType(const std::string& path)
{
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos)
        return "text/plain"; // default de text olsun, octet-stream yerine

    std::string ext = path.substr(dotPos + 1);

    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css")                  return "text/css";
    if (ext == "js")                   return "application/javascript"; // teknik olarak text ama MIME type'ı bu
    if (ext == "json")                 return "application/json";       // aynı şekilde text-tabanlı
    if (ext == "txt")                  return "text/plain";
    if (ext == "csv")                  return "text/csv";
    if (ext == "xml")                  return "application/xml";

    return "text/plain"; // bilinmeyen extension -> text/plain fallback
}

HttpResponse ResponseBuilder::handleGet(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    std::string filePath = resolveFilePath(request.getPath(), location);

    if (filePath.empty())          // path traversal denemesi
        return buildErrorResponse(403, serverConfig);

    if (!pathExists(filePath))
        return buildErrorResponse(404, serverConfig);

    if (isDirectory(filePath))
    {
        std::string dirPath = filePath;
        if (dirPath[dirPath.length() - 1] != '/')
            dirPath += "/";

        bool indexFound = false;
        if (!location.index.empty())
        {
            std::string indexPath = dirPath + location.index;
            if (pathExists(indexPath) && !isDirectory(indexPath))
            {
                filePath = indexPath;
                indexFound = true;
            }
        }

        if (!indexFound)
        {
            if (location.autoindex)
                return buildAutoindexPage(dirPath, request.getPath());
            return buildErrorResponse(403, serverConfig);
        }
    }

    std::string body;
    if (!readFile(filePath, body))
        return buildErrorResponse(500, serverConfig);

    HttpResponse response;
    response.setStatus(200);
    response.setHeader("Content-Type", getContentType(filePath));
    response.setBody(body); // HEAD ise body'yi serialize() aşamasında dışarıda bırak, burada aynı kalsın

    return response;
}


// dirPath: diskteki gerçek dizin path'i (resolveFilePath sonucu)
// requestPath: client'ın istediği URL (linkleri doğru üretmek için)
// Dizin içeriğini basit bir HTML listesi olarak döner (nginx autoindex benzeri).

HttpResponse ResponseBuilder::buildAutoindexPage(const std::string& dirPath, const std::string& requestPath)
{
    DIR* dir = opendir(dirPath.c_str());
    HttpResponse response;

    if (!dir)
    {
        response.setStatus(500);
        response.setHeader("Content-Type", "text/html");
        response.setBody("<html><body><h1>500 Internal Server Error</h1></body></html>");
        return response;
    }

    std::string urlPrefix = requestPath;
    if (urlPrefix.empty() || urlPrefix[urlPrefix.length() - 1] != '/')
        urlPrefix += "/";

    std::string diskPrefix = dirPath;
    if (diskPrefix.empty() || diskPrefix[diskPrefix.length() - 1] != '/')
        diskPrefix += "/";

    std::ostringstream html;
    html << "<html><head><title>Index of " << urlPrefix << "</title></head><body>";
    html << "<h1>Index of " << urlPrefix << "</h1><hr><pre>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == ".")
            continue; // kendi dizinini listeleme, ama ".." kalsın (üst dizine link)

        struct stat st;
        std::string childDiskPath = diskPrefix + name;
        bool isDir = (stat(childDiskPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode));

        html << "<a href=\"" << urlPrefix << name << (isDir ? "/" : "") << "\">"
             << name << (isDir ? "/" : "") << "</a>\n";
    }

    closedir(dir);
    html << "</pre><hr></body></html>";

    response.setStatus(200);
    response.setHeader("Content-Type", "text/html");
    response.setBody(html.str());
    return response;
}
