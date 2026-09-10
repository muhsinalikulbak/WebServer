#include "ResponseBuilder.hpp"
#include "RequestValidator.hpp"
#include "Router.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream> 
#include <sys/stat.h>
#include <dirent.h>   // opendir/readdir/closedir için

static HttpResponse buildStatusResponse(int statusCode, const std::string& locationHeader)
{
    // Ortak status cevabı üretmek için tek noktadan body/header kurar.
    // Redirect gibi durumlarda aynı HTML şablonunu tekrar tekrar yazmamak amaçlanır.
    // Location header yalnızca gerçekten gerekli olduğunda eklenir.
    HttpResponse response;
    std::ostringstream html;

    response.setStatus(statusCode);
    if (!locationHeader.empty())
        response.setHeader("Location", locationHeader);

    html << "<html><head><title>" << statusCode << "</title></head><body>"
         << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
         << "</body></html>";

    response.setHeader("Content-Type", "text/html");
    response.setBody(html.str());
    return response;
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
        return buildRedirect(*location);

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

HttpResponse ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
    // Hata cevaplarını tek tip üretmek için merkez fonksiyondur.
    // Önce config'teki özel error page dosyasını dener.
    // Dosya yoksa her zaman güvenli bir fallback HTML üretir.
    HttpResponse response;
    response.setStatus(statusCode);

    std::map<int, std::string>::const_iterator it = serverConfig.errorPages.find(statusCode);
    std::string body;
    bool loaded = false;

    // Config'te bu status için özel sayfa tanımlıysa onu yüklemeye çalışır.
    // Amaç kullanıcıya daha anlaşılır ve özelleştirilebilir hata çıktısı vermektir.
    if (it != serverConfig.errorPages.end())
        loaded = readFile(it->second, body);   // config'teki path'i doğrudan dene

    // Özel sayfa okunamazsa hata cevabını boş bırakmamak için fallback üretilir.
    // Böylece istemci her koşulda geçerli bir HTML body alır.
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

HttpResponse ResponseBuilder::buildRedirect(const LocationConfig& location)
{
    // Location return kuralını HTTP redirect cevabına çevirir.
    // Kodu ve hedef URL'yi tek noktadan üretmek davranış tutarlılığı sağlar.
    // 301/302 seçimi config üzerinden geldiği için burada sadece uygulanır.
    // 301 / 302 --- 301 Kalıcı, 302 Geçiçi yönlendirme olduğunu söyler.

    return buildStatusResponse(location.returnCode, location.returnUrl); // Güncel URL'dir.
}

bool ResponseBuilder::readFile(const std::string& path, std::string& outContent)
{
    // Dosya içeriğini binary olarak tek seferde belleğe taşır.
    // Binary mod, satır sonu dönüşümü gibi platform etkilerini önleyerek
    // gönderilecek içeriğin diskteki haliyle birebir korunmasını sağlar.
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
    // Burası üst dizine gitmesi durumunu yakalamak için mi.
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
    // Aynı path kontrolünü tekrar etmemek için küçük yardımcıdır.
    // Dosya mı dizin mi ayrımını burada yapmaz; yalnızca varlık bilgisini döner.
    // Bu sade ayrım üst katmanda doğru HTTP kararını vermeyi kolaylaştırır.
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

// stat() sonucundaki st_mode alanını S_ISDIR makrosuyla kontrol ederek path'in
// dizin mi olduğunu söyler. stat başarısızsa (yok/erişilemiyor) false döner.
bool ResponseBuilder::isDirectory(const std::string& path)
{
    // Path'in tipini (dizin mi) anlamak için stat bilgisini yorumlar.
    // Route davranışında dosya ve dizinin farklı ele alınması gerektiğinden
    // bu ayrım, 403/autoindex/index dosyası kararları için kritik önemdedir.
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

// Path'in extension'ına bakıp uygun MIME type döner.
// Bilinmeyen extension -> "application/octet-stream" (browser bunu indirir, bozuk render etmez).
std::string ResponseBuilder::getContentType(const std::string& path)
{
    // İstemciye doğru Content-Type vererek tarayıcı davranışını belirler.
    // Amaç dosyanın indirilmesi yerine mümkünse doğru şekilde render edilmesidir.
    // Bilinmeyen türlerde güvenli fallback ile cevabı yine de gönderilebilir tutar.
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos)
        return "text/plain"; // default de text olsun, octet-stream yerine

    // '.' dan sonrasını almak için yani uzantıyı almak için.
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
    if (!pathExists(location.uploadStore) || !isDirectory(location.uploadStore))
        return buildErrorResponse(500, serverConfig);

    const std::string& requestPath = request.getPath();
    size_t slashPos = requestPath.find_last_of('/');
    std::string filename;

    if (slashPos == std::string::npos)
        filename = requestPath;
    else
        filename = requestPath.substr(slashPos + 1);

    // Boş isim veya ".." içeren isim hem belirsiz hedefe hem traversal riskine yol açar.
    // Bu nedenle istemci girdisi geçersiz sayılarak 400 Bad Request döndürülür.
    if (filename.empty() || filename.find("..") != std::string::npos)
        return buildErrorResponse(400, serverConfig);

    std::string filePath = location.uploadStore;
    bool storeEndsSlash = !filePath.empty() && filePath[filePath.length() - 1] == '/';
    bool filenameStartsSlash = !filename.empty() && filename[0] == '/';

    if (storeEndsSlash && filenameStartsSlash)
        filePath.erase(filePath.length() - 1);
    else if (!storeEndsSlash && !filenameStartsSlash)
        filePath += "/";

    filePath += filename;

    // Hedef path bir dizine denk geliyorsa dosya üzerine yazma yapılamaz.
    // Kaynak mevcut olsa da işlem yetkisiz/uygunsuz olduğu için 403 seçilir.
    bool exists = pathExists(filePath);
    if (exists && isDirectory(filePath))
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
    HttpResponse response;
    int statusCode = alreadyExists ? 200 : 201;
    response.setStatus(statusCode);
    // Location sadece yeni kaynak oluşturulduğunda istemciye canonical yolu bildirmek için eklenir.
    if (!alreadyExists)
        response.setHeader("Location", request.getPath());

    std::ostringstream html;
    html << "<html><head><title>" << statusCode << "</title></head><body>"
         << "<center><h1>" << statusCode << " " << HttpResponse::statusTextFor(statusCode) << "</h1></center>"
         << "</body></html>";

    response.setHeader("Content-Type", "text/html");
    response.setBody(html.str());
    return response;
}


HttpResponse ResponseBuilder::handleDelete(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // DELETE isteğinde hedef dosyayı güvenli biçimde kaldırmayı amaçlar.
    // Önce URL->disk çözümlemesi ve varlık tipi doğrulanır, sonra silme denenir.
    // errno tabanlı ayrım ile istemci hatası ve sunucu hatası birbirinden ayrılır.
	HttpResponse response;
	std::string filePath = resolveFilePath(request.getPath(), location);

    // Çözümleme başarısızsa (örn. traversal) erişim ihlali kabul edilip 403 dönülür.
	if (filePath.empty())
		return buildErrorResponse(403, serverConfig);
	
    // Silinecek kaynak yoksa doğru semantik 404'tür.
	if (!pathExists(filePath))
		return buildErrorResponse(404, serverConfig);

	// Dizin silmek yani recursive delete riskli ve zaten zorunlu değil
	// o yüzden directory silme olmayacak.
    // Dizin silmeyi kapatmak, recursive delete riskini ve beklenmeyen veri kaybını önler.
    // Bu nedenle dizin hedefinde işlem reddedilir ve 403 dönülür.
	if (isDirectory(filePath))
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
		return buildErrorResponse(500, serverConfig); // Internal server error.
	
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
    if (!pathExists(filePath))
        return buildErrorResponse(404, serverConfig);

    if (isDirectory(filePath))
    {
        // Relative link'lerin browser tarafından yanlış base URL ile çözülmesini engellemek için,
        // nginx'in yaptığı gibi slash olmadan gelen dizin isteklerini 301 ile "/" eklenmiş URL'ye yönlendiriyoruz.
        // Dizine slash ile yönlendirme, relative asset linklerinin bozulmaması için kritiktir.
        // Bu yüzden kalıcı URL normalizasyonu olarak 301 tercih edilir.
        if (request.getPath().empty() || request.getPath()[request.getPath().length() - 1] != '/')
            return buildStatusResponse(301, request.getPath() + "/");

        // Direction olduğu için, filePath değil artık dirPath olarak işlev görür.
        // Buraya gerek aslında, sadece ekstra ekstra kontrol için.
        std::string dirPath = filePath;
        if (dirPath[dirPath.length() - 1] != '/')
            dirPath += "/";

        bool indexFound = false;
        if (!location.index.empty())
        {
            // var/www/images/index.html
            // Index dosyası varsa dizin görünümü yerine onu sunmak
            // klasik web server beklentisini korur.
            std::string indexPath = dirPath + location.index;
            if (pathExists(indexPath) && !isDirectory(indexPath))
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
    if (!readFile(filePath, body))
        return buildErrorResponse(500, serverConfig);

    HttpResponse response;
    // Kaynak başarıyla bulundu ve üretildiğinde standart başarı kodu 200'dür.
    response.setStatus(200);
    response.setHeader("Content-Type", getContentType(filePath));
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
    std::string urlPrefix = requestPath;
    if (urlPrefix.empty() || urlPrefix[urlPrefix.length() - 1] != '/')
        urlPrefix += "/";

    // Disk prefix normalizasyonu, child path üretiminde çift/eksik slash
    // kaynaklı stat hatalarını engellemek için yapılır.
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
