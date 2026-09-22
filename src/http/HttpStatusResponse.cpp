#include "HttpStatusResponse.hpp"
#include "LocationConfig.hpp"
#include <sstream>

// Shared HTML gövdesini üretir; hem error fallback'i hem generic status cevabı
// (redirect dahil) aynı şablonu kullanır, böylece kopyalar sürüklenmez (drift engeli).
std::string HttpStatusResponse::html(int statusCode)
{
    std::ostringstream ss;
    ss << "<html><head><title>" << statusCode << "</title></head><body>"
       << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
       << "</body></html>";
    return ss.str();
}

// Ortak status cevabı üretmek için tek noktadan body/header kurar.
// Redirect gibi durumlarda aynı HTML şablonunu tekrar tekrar yazmamak amaçlanır.
// Location header yalnızca gerçekten gerekli olduğunda eklenir.
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

// Location return kuralını HTTP redirect cevabına çevirir.
// Kodu ve hedef URL'yi tek noktadan üretmek davranış tutarlılığı sağlar.
// 301/302 seçimi config üzerinden geldiği için burada sadece uygulanır.
// 301 / 302 --- 301 Kalıcı, 302 Geçiçi yönlendirme olduğunu söyler.
HttpResponse HttpStatusResponse::redirect(const LocationConfig& location)
{
    return build(location.returnCode, location.returnUrl); // Güncel URL'dir.
}