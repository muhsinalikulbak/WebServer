#include "ErrorResponse.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include "FileUtils.hpp"
#include <sstream>
#include <map>

namespace
{
    // status cevabı ve hata fallback'inde birebir tekrarlanan HTML şablonunu tek
    // yerden üretir; kopyaların zamanla sürüklenmesini (drift) engeller.
    std::string buildStatusHtml(int statusCode)
    {
        std::ostringstream html;
        html << "<html><head><title>" << statusCode << "</title></head><body>"
             << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
             << "</body></html>";
        return html.str();
    }

    // Ortak status cevabı üretmek için tek noktadan body/header kurar.
    // Redirect gibi durumlarda aynı HTML şablonunu tekrar tekrar yazmamak amaçlanır.
    // Location header yalnızca gerçekten gerekli olduğunda eklenir.
    HttpResponse buildStatusResponse(int statusCode, const std::string& locationHeader)
    {
        HttpResponse response;
        std::string body = buildStatusHtml(statusCode);

        response.setStatus(statusCode);
        if (!locationHeader.empty())
            response.setHeader("Location", locationHeader);

        response.setHeader("Content-Type", "text/html");
        response.setBody(body);
        return response;
    }
}

namespace ErrorResponse
{
    // Hata cevaplarını tek tip üretmek için merkez fonksiyondur.
    // Önce config'teki özel error page dosyasını dener.
    // Dosya yoksa her zaman güvenli bir fallback HTML üretir.
    HttpResponse build(int statusCode, const ServerConfig& serverConfig)
    {
        HttpResponse response;
        response.setStatus(statusCode);

        std::map<int, std::string>::const_iterator it = serverConfig.errorPages.find(statusCode);
        std::string body;
        bool loaded = false;

        // Config'te bu status için özel sayfa tanımlıysa onu yüklemeye çalışır.
        // Amaç kullanıcıya daha anlaşılır ve özelleştirilebilir hata çıktısı vermektir.
        if (it != serverConfig.errorPages.end())
            loaded = FileUtils::readFile(it->second, body);   // config'teki path'i doğrudan dene

        // Özel sayfa okunamazsa hata cevabını boş bırakmamak için fallback üretilir.
        // Böylece istemci her koşulda geçerli bir HTML body alır.
        if (!loaded)
            body = buildStatusHtml(statusCode);

        response.setHeader("Content-Type", "text/html");
        response.setBody(body);
        return response;
    }

    // Location return kuralını HTTP redirect cevabına çevirir.
    // Kodu ve hedef URL'yi tek noktadan üretmek davranış tutarlılığı sağlar.
    // 301/302 seçimi config üzerinden geldiği için burada sadece uygulanır.
    // 301 / 302 --- 301 Kalıcı, 302 Geçiçi yönlendirme olduğunu söyler.
    HttpResponse redirect(const LocationConfig& location)
    {
        return buildStatusResponse(location.returnCode, location.returnUrl); // Güncel URL'dir.
    }
}