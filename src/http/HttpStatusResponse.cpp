#include "HttpStatusResponse.hpp"
#include "LocationConfig.hpp"
#include <sstream>

// Ortak status HTML gövdesini ("<title>KOD</title>" + "<h1>KOD AÇIKLAMA</h1>") üretir.
std::string HttpStatusResponse::html(int statusCode)
{
    std::ostringstream ss;
    ss << "<html><head><title>" << statusCode << "</title></head><body>"
       << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
       << "</body></html>";
    return ss.str();
}

// Verilen status kodu ve opsiyonel Location header'ı ile genel bir HTTP yanıtı oluşturur.
HttpResponse HttpStatusResponse::build(int statusCode, const std::string& locationHeader)
{
    HttpResponse response;
    std::string body = html(statusCode);

    response.setStatus(statusCode);
    if (!locationHeader.empty())
        response.setHeader("Location", locationHeader);

    response.setHeader("Content-Type", "text/html");
    response.setBody(body);
    return response;
}

// Location konfigürasyonundaki return kuralını HTTP redirect yanıtına çevirir.
HttpResponse HttpStatusResponse::redirect(const LocationConfig& location)
{
    return build(location.returnCode, location.returnUrl);
}