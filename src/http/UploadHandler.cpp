#include "UploadHandler.hpp"
#include "ResponseBuilder.hpp"
#include "FileUtils.hpp"
#include "HttpStatusResponse.hpp"
#include <string>
#include <fstream>
#include <cerrno>

namespace
{
    // Upload hedef dosya yolunu çözer; uploadStore/dosya adı geçersizse errorOut'u doldurup false döner.
    bool resolveUploadTarget(const HttpRequest& request, const LocationConfig& location,
                             const ServerConfig& serverConfig, std::string& outFilePath,
                             HttpResponse& errorOut)
    {
        if (location.uploadStore.empty())
        {
            errorOut = ResponseBuilder::buildErrorResponse(403, serverConfig);
            return false;
        }

        std::string filename = FileUtils::lastPathSegment(request.getPath());

        if (filename.empty() || filename.find("..") != std::string::npos)
        {
            errorOut = ResponseBuilder::buildErrorResponse(400, serverConfig);
            return false;
        }

        outFilePath = FileUtils::joinPath(location.uploadStore, filename);
        return true;
    }
}

// POST isteğinin gövdesini uploadStore altına dosya olarak yazar (yeni: 201, üzerine yazma: 200).
HttpResponse UploadHandler::post(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    std::string filePath;
    HttpResponse errorOut;
    if (!resolveUploadTarget(request, location, serverConfig, filePath, errorOut))
        return errorOut;

    if (!FileUtils::pathExists(location.uploadStore) || !FileUtils::isDirectory(location.uploadStore))
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    bool exists = FileUtils::pathExists(filePath);
    if (exists && FileUtils::isDirectory(filePath))
        return ResponseBuilder::buildErrorResponse(403, serverConfig);

    bool alreadyExists = exists;

    std::ofstream out(filePath.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    const std::string& data = request.getBody();
    out.write(data.data(), data.size());
    out.close();

    if (out.fail())
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    int statusCode = alreadyExists ? 200 : 201;
    return HttpStatusResponse::build(statusCode, alreadyExists ? "" : request.getPath());
}

// DELETE isteğiyle hedeflenen upload dosyasını diskten kaldırır.
HttpResponse UploadHandler::remove(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    std::string filePath;
    HttpResponse errorOut;
    if (!resolveUploadTarget(request, location, serverConfig, filePath, errorOut))
        return errorOut;

    HttpResponse response;

    if (!FileUtils::pathExists(filePath))
        return ResponseBuilder::buildErrorResponse(404, serverConfig);

    if (FileUtils::isDirectory(filePath))
        return ResponseBuilder::buildErrorResponse(403, serverConfig);

    if (std::remove(filePath.c_str()) == 0)
    {
        response.setStatus(204);
    }
    else if (errno == EACCES || errno == EPERM)
        return ResponseBuilder::buildErrorResponse(403, serverConfig);
    else if (errno == ENOENT)
        return ResponseBuilder::buildErrorResponse(404, serverConfig);
    else
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    return response;
}