#include "StaticHandler.hpp"
#include "ResponseBuilder.hpp"
#include "FileUtils.hpp"
#include "MimeTypes.hpp"
#include "HttpStatusResponse.hpp"
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>

namespace
{
    // Dizin içeriğini basit bir HTML listesi (nginx autoindex benzeri) olarak döner.
    HttpResponse buildAutoindexPage(const std::string& dirPath, const std::string& requestPath)
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

        std::string urlPrefix = FileUtils::withTrailingSlash(requestPath);

        std::string diskPrefix = FileUtils::withTrailingSlash(dirPath);

        std::ostringstream html;
        html << "<html><head><title>Index of " << urlPrefix << "</title></head><body>";
        html << "<h1>Index of " << urlPrefix << "</h1><hr><pre>";

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL)
        {
            std::string name = entry->d_name;
            if (name == ".")
                continue;

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
}

// GET isteğine karşılık hedef dosyayı, dizin index'ini ya da autoindex listesini döner.
HttpResponse StaticHandler::get(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    std::string filePath = ResponseBuilder::resolveFilePath(request.getPath(), location);

    if (filePath.empty())
        return ResponseBuilder::buildErrorResponse(403, serverConfig);

    if (!FileUtils::pathExists(filePath))
        return ResponseBuilder::buildErrorResponse(404, serverConfig);

    if (FileUtils::isDirectory(filePath))
    {
        if (request.getPath().empty() || request.getPath()[request.getPath().length() - 1] != '/')
        {
            return HttpStatusResponse::build(301, request.getPath() + "/");
        }

        std::string dirPath = FileUtils::withTrailingSlash(filePath);

        bool indexFound = false;
        if (!location.index.empty())
        {
            std::string indexPath = dirPath + location.index;
            if (FileUtils::pathExists(indexPath) && !FileUtils::isDirectory(indexPath))
            {
                filePath = indexPath;
                indexFound = true;
            }
        }

        if (!indexFound)
        {
            if (location.autoindex)
                return buildAutoindexPage(dirPath, request.getPath());
            return ResponseBuilder::buildErrorResponse(404, serverConfig);
        }
    }

    std::string body;
    if (!FileUtils::readFile(filePath, body))
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    HttpResponse response;
    response.setStatus(200);
    response.setHeader("Content-Type", MimeTypes::fromPath(filePath));
    response.setBody(body);

    return response;
}