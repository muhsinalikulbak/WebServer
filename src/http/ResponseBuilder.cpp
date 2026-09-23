#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"
#include "FileUtils.hpp"
#include "MimeTypes.hpp"
#include "ErrorResponse.hpp"
#include "HttpStatusResponse.hpp"
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

// sonucuna göre uygun dala (error / redirect / GET / POST / DELETE) dallanılır.
HttpResponse ResponseBuilder::build(const HttpRequest& request, const ServerConfig& serverConfig)
{
    // Bu fonksiyon istek işleme akışının yönlendiricisidir.
    // Önce request semantiğini doğrular, sonra location eşleşmesini bulur.
    // Ardından method/özel durumlara göre doğru handler'a dallanır.
    int validationCode = RequestValidator::validate(request);

    // Validator bir kod döndürdüyse istemci hatası/protokol ihlali vardır,
    // request işleme devam etmek yerine doğrudan ilgili hata cevabı dönülür.
    if (validationCode)
        return buildErrorResponse(validationCode, serverConfig);

    const LocationConfig* location = Router::match(request.getPath(), serverConfig);
    
    // Hiçbir location eşleşmezse kaynak bulunamadı kabul edilir ve 404 dönülür.
    // Bu seçim routing seviyesinde "URL bu server'da yok" anlamını taşır.
    if (!location)
        return buildErrorResponse(404, serverConfig);

    // return direktifi metoddan bağımsız çalışır (nginx semantiği) -> method check'ten önce
    if (location->returnCode != 0)
        return HttpStatusResponse::redirect(*location);

    // Method bu location için izinli değilse 405 dönülür.
    // Çünkü kaynak var, fakat o kaynakta bu HTTP method'u desteklenmiyor.
    if (!isMethodAllowedForLocation(request.getMethod(), *location))
        return buildErrorResponse(405, serverConfig);

    std::string cgiExt;
    // CGI yolu tespit edilip henüz implementasyon yoksa 501 seçilir.
    // Bu kod "server özelliği tanıyor ama desteklemiyor" semantiğini verir.
    if (isCgiRequest(request.getPath(), *location, cgiExt))
        return buildErrorResponse(501, serverConfig); // CGI fazı henüz yok

    if (request.getMethod() == "get")
        return handleGet(request, *location, serverConfig);
    else if (request.getMethod() == "post")
        return handlePost(request, *location, serverConfig);
    else if (request.getMethod() == "delete")
        return handleDelete(request, *location, serverConfig);

    // Bu noktaya düşmek, method ailesinin teorik olarak desteklenmediği anlamına gelir.
    // 501 ile cevaplayarak istemciye sunucu tarafında özellik eksikliği bildirilir.
    return buildErrorResponse(501, serverConfig);
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

HttpResponse ResponseBuilder::handlePost(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // POST gövdesini uploadStore altına dosya olarak kaydeder.
    // Akışın amacı hem path güvenliğini korumak hem de dosya yazım hatalarını
    // doğru HTTP status kodlarıyla istemciye anlaşılır biçimde yansıtmaktır.

    // uploadStore tanımsızsa bu location upload'a izin vermiyor kabul edilir.
    // Bu yüzden erişim reddi semantiğiyle 403 dönülür.
    if (location.uploadStore.empty())
        return buildErrorResponse(403, serverConfig);

    // Upload hedefi diskte yoksa ya da dizin değilse istemci değil config hatasıdır.
    // Sunucu yanlış yapılandırıldığı için 500 Internal Server Error seçilir.
    if (!FileUtils::pathExists(location.uploadStore) || !FileUtils::isDirectory(location.uploadStore))
        return buildErrorResponse(500, serverConfig);

    // upload file adı, URL'nin son path parçasıdır (FileUtils'te ortaklaşır).
    std::string filename = FileUtils::lastPathSegment(request.getPath());

    // Boş isim veya ".." içeren isim hem belirsiz hedefe hem traversal riskine yol açar.
    // Bu nedenle istemci girdisi geçersiz sayılarak 400 Bad Request döndürülür.
    if (filename.empty() || filename.find("..") != std::string::npos)
        return buildErrorResponse(400, serverConfig);

    // filename hiçbir zaman '/' ile başlamaz (lastPathSegment sonrası), dolayısıyla
    // joinPath uploadStore ile aynı sonucu verir: varsa tek slash, yoksa "store/name".
    std::string filePath = FileUtils::joinPath(location.uploadStore, filename);

    // Hedef path bir dizine denk geliyorsa dosya üzerine yazma yapılamaz.
    // Kaynak mevcut olsa da işlem yetkisiz/uygunsuz olduğu için 403 seçilir.
    bool exists = FileUtils::pathExists(filePath);
    if (exists && FileUtils::isDirectory(filePath))
        return buildErrorResponse(403, serverConfig);

    bool alreadyExists = exists;

    // Dosya açılamıyorsa yazma aşamasına geçmek mümkün değildir.
    // Bu durum sunucu tarafı I/O problemi olduğu için 500 ile raporlanır.
    std::ofstream out(filePath.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return buildErrorResponse(500, serverConfig);

    const std::string& data = request.getBody();
    out.write(data.data(), data.size());
    out.close();

    // Yazma sonrası fail kontrolü, disk dolu/izin gibi geç yakalanan I/O hatalarını
    // istemciye doğru iletmek için zorunludur; aksi halde sahte başarı üretilebilir.
    if (out.fail())
        return buildErrorResponse(500, serverConfig);

    // Yeni oluşturulan kaynakta 201 dönülerek resource creation semantiği korunur.
    // Var olan dosya üzerine yazmada yalnızca içerik güncellendiği için 200 yeterlidir.
    int statusCode = alreadyExists ? 200 : 201;
    // Location sadece yeni kaynak oluşturulduğunda istemciye canonical yolu bildirmek için eklenir.
    // Varlık durumunda boş Location, HttpStatusResponse::build'te header'ın eklenmemesini sağlar.
    return HttpStatusResponse::build(statusCode, alreadyExists ? "" : request.getPath());
}


HttpResponse ResponseBuilder::handleDelete(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // DELETE isteğinde hedef dosyayı güvenli biçimde kaldırmayı amaçlar.
    // POST ile aynı mantık: URL'den dosya adını çıkar, uploadStore ile birleştir.
    // errno tabanlı ayrım ile istemci hatası ve sunucu hatası birbirinden ayrılır.
	HttpResponse response;

    // uploadStore tanımsızsa bu location delete'e izin vermiyor kabul edilir.
    if (location.uploadStore.empty())
		return buildErrorResponse(403, serverConfig);

    // Silinecek dosya adı, URL'nin son path parçasıdır (FileUtils'te ortaklaşır).
    std::string filename = FileUtils::lastPathSegment(request.getPath());

    // Boş isim veya ".." içeren isim hem belirsiz hedefe hem traversal riskine yol açar.
    if (filename.empty() || filename.find("..") != std::string::npos)
        return buildErrorResponse(400, serverConfig);

    // filename hiçbir zaman '/' ile başlamaz (lastPathSegment sonrası), dolayısıyla
    // joinPath uploadStore ile aynı sonucu verir: varsa tek slash, yoksa "store/name".
    std::string filePath = FileUtils::joinPath(location.uploadStore, filename);

    // Silinecek kaynak yoksa doğru semantik 404'tür.
	if (!FileUtils::pathExists(filePath))
		return buildErrorResponse(404, serverConfig);

	// Dizin silmek riskli, bu nedenle dizin hedefinde işlem reddedilir.
	if (FileUtils::isDirectory(filePath))
		return buildErrorResponse(403, serverConfig);
	
	if (std::remove(filePath.c_str()) == 0)
	{
        // Başarılı silmede body gerekmeyen durum kodu olarak 204 uygundur.
		response.setStatus(204);
	}
    // İzin/yetki engeli olduğunda kaynak olsa bile işlem yasak olduğu için 403 döner.
	else if (errno == EACCES || errno == EPERM)
		return buildErrorResponse(403, serverConfig);
    // Data race gibi sebeplerle dosya artık yoksa istemciye 404 bildirilir.
	else if (errno == ENOENT)
		return buildErrorResponse(404, serverConfig);
    // Yukarıdakiler dışındaki işletim sistemi hataları sunucu iç hata sınıfına girer.
	else
		return buildErrorResponse(500, serverConfig);
	
	return response;
		
}


HttpResponse ResponseBuilder::handleGet(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // GET/HEAD için hedef kaynağı bulup doğru temsilini döner.
    // Dosya, dizin, index ve autoindex senaryolarını ayırarak
    // web sunucusunun beklenen URL davranışını korumayı amaçlar.

    // url ile root path'i birleştirir.
    std::string filePath = resolveFilePath(request.getPath(), location);

    // Çözümleme geçersizse (özellikle traversal) güvenlik gereği 403 dönülür.
    if (filePath.empty())          // path traversal denemesi
        return buildErrorResponse(403, serverConfig);

    // Hedef yoksa istemci yanlış URL istemiştir; doğru yanıt 404'tür.
    if (!FileUtils::pathExists(filePath))
        return buildErrorResponse(404, serverConfig);

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
            return buildErrorResponse(403, serverConfig);
        }
    }

    std::string body;
    // Okunabilirlik/izin/I/O problemi varsa sunucu dosyayı temsil edemez;
    // bu nedenle istemciye 500 Internal Server Error gönderilir.
    if (!FileUtils::readFile(filePath, body))
        return buildErrorResponse(500, serverConfig);

    HttpResponse response;
    // Kaynak başarıyla bulundu ve üretildiğinde standart başarı kodu 200'dür.
    response.setStatus(200);
    response.setHeader("Content-Type", MimeTypes::fromPath(filePath));
    response.setBody(body);

    return response;
}


// dirPath: diskteki gerçek dizin path'i (resolveFilePath sonucu)
// requestPath: client'ın istediği URL (linkleri doğru üretmek için)
// Dizin içeriğini basit bir HTML listesi olarak döner (nginx autoindex benzeri).

HttpResponse ResponseBuilder::buildAutoindexPage(const std::string& dirPath, const std::string& requestPath)
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
