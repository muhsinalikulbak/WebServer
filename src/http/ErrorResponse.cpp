#include "ErrorResponse.hpp"
#include "HttpStatusResponse.hpp"
#include "ServerConfig.hpp"
#include "FileUtils.hpp"
#include <map>

namespace ErrorResponse
{
    // İsteğin karşılığında config'teki özel hata sayfasını, yoksa fallback HTML'i döner.
    HttpResponse build(int statusCode, const ServerConfig& serverConfig)
    {
        HttpResponse response;
        response.setStatus(statusCode);

        std::map<int, std::string>::const_iterator it = serverConfig.errorPages.find(statusCode);
        std::string body;
        bool loaded = false;

        if (it != serverConfig.errorPages.end())
            loaded = FileUtils::readFile(it->second, body);

        if (!loaded)
            body = HttpStatusResponse::html(statusCode);

        response.setHeader("Content-Type", "text/html");
        response.setBody(body);
        return response;
    }
}