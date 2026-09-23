#include "StaticHandler.hpp"
#include "ResponseBuilder.hpp"
#include "FileUtils.hpp"
#include "MimeTypes.hpp"
#include "HttpStatusResponse.hpp"
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>   // opendir/readdir/closedir için

namespace
{
    // dirPath: diskteki gerçek dizin path'i (resolveFilePath sonucu)
    // requestPath: client'ın istediği URL (linkleri doğru üretmek için)
    // Dizin içeriğini basit bir HTML listesi olarak döner (nginx autoindex benzeri).

    HttpResponse buildAutoindexPage(const std::string& dirPath, const std::string& requestPath)
    {
        // Dizin içeriğinden dinamik bir HTML index sayfası üretir.
        // Amaç, autoindex açıkken istemcinin dizin altındaki kaynakları
        // tarayıcı üzerinden güvenli ve basit bir liste halinde görebilmesidir.
        DIR* dir = opendir(dirPath.c_str());
        HttpResponse response;

        // Dizin açılamıyorsa listeleme üretilemez; bu bir sunucu tarafı erişim/I/O sorunudur.
        // Bu yüzden 500 Internal Server Error döndürülür.
        if (!dir)
        {
            response.setStatus(500);
            response.setHeader("Content-Type", "text/html");
            response.setBody("<html><body><h1>500 Internal Server Error</h1></body></html>");
            return response;
        }

        // URL prefix'ini slash ile normalize etmek, üretilen linklerin
        // hem dosya hem dizin öğelerinde tutarlı olmasını sağlar.
        std::string urlPrefix = FileUtils::withTrailingSlash(requestPath);

        // Disk prefix normalizasyonu, child path üretiminde çift/eksik slash
        // kaynaklı stat hatalarını engellemek için yapılır.
        std::string diskPrefix = FileUtils::withTrailingSlash(dirPath);

        std::ostringstream html;
        html << "<html><head><title>Index of " << urlPrefix << "</title></head><body>";
        html << "<h1>Index of " << urlPrefix << "</h1><hr><pre>";

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL)
        {
            std::string name = entry->d_name;
            // "." kaydını gizlemek, aynı dizine anlamsız tekrar link üretimini önler.
            // ".." kaydı bırakılarak üst dizine geri çıkış davranışı korunur.
            if (name == ".")
                continue; // kendi dizinini listeleme, ama ".." kalsın (üst dizine link)

            struct stat st;
            std::string childDiskPath = diskPrefix + name;
            bool isDir = (stat(childDiskPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode));

            // Dizin öğelerine trailing slash eklemek, tarayıcının bunu klasör olarak
            // yorumlamasını sağlar ve relative çözümlemeleri doğru tutar.
            html << "<a href=\"" << urlPrefix << name << (isDir ? "/" : "") << "\">"
                 << name << (isDir ? "/" : "") << "</a>\n";
        }

        closedir(dir);
        html << "</pre><hr></body></html>";

        // Autoindex başarıyla üretildiğinde kaynak temsili hazırdır, bu yüzden 200 döner.
        response.setStatus(200);
        response.setHeader("Content-Type", "text/html");
        response.setBody(html.str());
        return response;
    }
}

HttpResponse StaticHandler::get(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // GET/HEAD için hedef kaynağı bulup doğru temsilini döner.
    // Dosya, dizin, index ve autoindex senaryolarını ayırarak
    // web sunucusunun beklenen URL davranışını korumayı amaçlar.

    // url ile root path'i birleştirir.
    std::string filePath = ResponseBuilder::resolveFilePath(request.getPath(), location);

    // Çözümleme geçersizse (özellikle traversal) güvenlik gereği 403 dönülür.
    if (filePath.empty())          // path traversal denemesi
        return ResponseBuilder::buildErrorResponse(403, serverConfig);

    // Hedef yoksa istemci yanlış URL istemiştir; doğru yanıt 404'tür.
    if (!FileUtils::pathExists(filePath))
        return ResponseBuilder::buildErrorResponse(404, serverConfig);

    if (FileUtils::isDirectory(filePath))
    {
        // Relative link'lerin browser tarafından yanlış base URL ile çözülmesini engellemek için,
        // nginx'in yaptığı gibi slash olmadan gelen dizin isteklerini 301 ile "/" eklenmiş URL'ye yönlendiriyoruz.
        // Dizine slash ile yönlendirme, relative asset linklerinin bozulmaması için kritiktir.
        // Bu yüzden kalıcı URL normalizasyonu olarak 301 tercih edilir.
        if (request.getPath().empty() || request.getPath()[request.getPath().length() - 1] != '/')
        {
            // Bu config'ten gelen bir redirect değil, trailing-slash normalizasyonu; bu yüzden
            // location.returnCode/returnUrl uydurmak yerine status kodu ve hedefi doğrudan veriyoruz.
            return HttpStatusResponse::build(301, request.getPath() + "/");
        }

        // Direction olduğu için, filePath değil artık dirPath olarak işlev görür.
        // Buraya gerek aslında, sadece ekstra ekstra kontrol için. ******
        // filePath bir dizin olduğundan boş değildir; trailing slash FileUtils'te eklenir.
        std::string dirPath = FileUtils::withTrailingSlash(filePath);

        bool indexFound = false;
        if (!location.index.empty())
        {
            // var/www/images/index.html
            // Index dosyası varsa dizin görünümü yerine onu sunmak
            // klasik web server beklentisini korur.
            std::string indexPath = dirPath + location.index;
            if (FileUtils::pathExists(indexPath) && !FileUtils::isDirectory(indexPath))
            {
                filePath = indexPath;
                indexFound = true;
            }
        }

        if (!indexFound)
        {
            // Autoindex açıksa dizin listesi üretmek kullanıcıya keşif imkanı verir.
            // Kapalıysa dizin içeriği ifşasını önlemek için 403 dönülür.
            if (location.autoindex)
                return buildAutoindexPage(dirPath, request.getPath());
            return ResponseBuilder::buildErrorResponse(403, serverConfig);
        }
    }

    std::string body;
    // Okunabilirlik/izin/I/O problemi varsa sunucu dosyayı temsil edemez;
    // bu nedenle istemciye 500 Internal Server Error gönderilir.
    if (!FileUtils::readFile(filePath, body))
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    HttpResponse response;
    // Kaynak başarıyla bulundu ve üretildiğinde standart başarı kodu 200'dür.
    response.setStatus(200);
    response.setHeader("Content-Type", MimeTypes::fromPath(filePath));
    response.setBody(body);

    return response;
}