#include "ErrorResponse.hpp"
#include "HttpStatusResponse.hpp"
#include "ServerConfig.hpp"
#include "FileUtils.hpp"
#include <map>

// Ortak status/redirect cevapları HttpStatusResponse modülünde tutulur; bu modül
// yalnızca config'e bağlı GERÇEK hata sayfasını üretir (error_page + fallback).
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
            body = HttpStatusResponse::html(statusCode);

        response.setHeader("Content-Type", "text/html");
        response.setBody(body);
        return response;
    }
}