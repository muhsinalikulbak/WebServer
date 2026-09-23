#include "UploadHandler.hpp"
#include "ResponseBuilder.hpp"
#include "FileUtils.hpp"
#include "HttpStatusResponse.hpp"
#include <string>
#include <fstream>
#include <cerrno>

namespace
{
    // POST ve DELETE ortak başlangıç adımları: uploadStore tanımsızsa 403,
    // hedef dosya adı boşsa/".." içeriyorsa 400. Başarılıysa outFilePath doldurulur.
    // true = devam edilebilir; false = hazır errorOut döndürülmeli (çağıran iletir).
    bool resolveUploadTarget(const HttpRequest& request, const LocationConfig& location,
                             const ServerConfig& serverConfig, std::string& outFilePath,
                             HttpResponse& errorOut)
    {
        // uploadStore tanımsızsa bu location upload'a izin vermiyor kabul edilir.
        // Bu yüzden erişim reddi semantiğiyle 403 dönülür.
        if (location.uploadStore.empty())
        {
            errorOut = ResponseBuilder::buildErrorResponse(403, serverConfig);
            return false;
        }

        // upload file adı, URL'nin son path parçasıdır (FileUtils'te ortaklaşır).
        std::string filename = FileUtils::lastPathSegment(request.getPath());

        // Boş isim veya ".." içeren isim hem belirsiz hedefe hem traversal riskine yol açar.
        // Bu nedenle istemci girdisi geçersiz sayılarak 400 Bad Request döndürülür.
        if (filename.empty() || filename.find("..") != std::string::npos)
        {
            errorOut = ResponseBuilder::buildErrorResponse(400, serverConfig);
            return false;
        }

        // filename hiçbir zaman '/' ile başlamaz (lastPathSegment sonrası), dolayısıyla
        // joinPath uploadStore ile aynı sonucu verir: varsa tek slash, yoksa "store/name".
        outFilePath = FileUtils::joinPath(location.uploadStore, filename);
        return true;
    }
}

HttpResponse UploadHandler::post(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // POST gövdesini uploadStore altına dosya olarak kaydeder.
    // Akışın amacı hem path güvenliğini korumak hem de dosya yazım hatalarını
    // doğru HTTP status kodlarıyla istemciye anlaşılır biçimde yansıtmaktır.

    std::string filePath;
    HttpResponse errorOut;
    if (!resolveUploadTarget(request, location, serverConfig, filePath, errorOut))
        return errorOut;

    // Upload hedefi diskte yoksa ya da dizin değilse istemci değil config hatasıdır.
    // Sunucu yanlış yapılandırıldığı için 500 Internal Server Error seçilir.
    if (!FileUtils::pathExists(location.uploadStore) || !FileUtils::isDirectory(location.uploadStore))
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    // Hedef path bir dizine denk geliyorsa dosya üzerine yazma yapılamaz.
    // Kaynak mevcut olsa da işlem yetkisiz/uygunsuz olduğu için 403 seçilir.
    bool exists = FileUtils::pathExists(filePath);
    if (exists && FileUtils::isDirectory(filePath))
        return ResponseBuilder::buildErrorResponse(403, serverConfig);

    bool alreadyExists = exists;

    // Dosya açılamıyorsa yazma aşamasına geçmek mümkün değildir.
    // Bu durum sunucu tarafı I/O problemi olduğu için 500 ile raporlanır.
    std::ofstream out(filePath.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    const std::string& data = request.getBody();
    out.write(data.data(), data.size());
    out.close();

    // Yazma sonrası fail kontrolü, disk dolu/izin gibi geç yakalanan I/O hatalarını
    // istemciye doğru iletmek için zorunludur; aksi halde sahte başarı üretilebilir.
    if (out.fail())
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    // Yeni oluşturulan kaynakta 201 dönülerek resource creation semantiği korunur.
    // Var olan dosya üzerine yazmada yalnızca içerik güncellendiği için 200 yeterlidir.
    int statusCode = alreadyExists ? 200 : 201;
    // Location sadece yeni kaynak oluşturulduğunda istemciye canonical yolu bildirmek için eklenir.
    // Varlık durumunda boş Location, HttpStatusResponse::build'te header'ın eklenmemesini sağlar.
    return HttpStatusResponse::build(statusCode, alreadyExists ? "" : request.getPath());
}

HttpResponse UploadHandler::remove(const HttpRequest& request, const LocationConfig& location, const ServerConfig& serverConfig)
{
    // DELETE isteğinde hedef dosyayı güvenli biçimde kaldırmayı amaçlar.
    // POST ile aynı mantık: URL'den dosya adını çıkar, uploadStore ile birleştir.
    // errno tabanlı ayrım ile istemci hatası ve sunucu hatası birbirinden ayrılır.

    std::string filePath;
    HttpResponse errorOut;
    if (!resolveUploadTarget(request, location, serverConfig, filePath, errorOut))
        return errorOut;

    HttpResponse response;

    // Silinecek kaynak yoksa doğru semantik 404'tür.
    if (!FileUtils::pathExists(filePath))
        return ResponseBuilder::buildErrorResponse(404, serverConfig);

    // Dizin silmek riskli, bu nedenle dizin hedefinde işlem reddedilir.
    if (FileUtils::isDirectory(filePath))
        return ResponseBuilder::buildErrorResponse(403, serverConfig);

    if (std::remove(filePath.c_str()) == 0)
    {
        // Başarılı silmede body gerekmeyen durum kodu olarak 204 uygundur.
        response.setStatus(204);
    }
    // İzin/yetki engeli olduğunda kaynak olsa bile işlem yasak olduğu için 403 döner.
    else if (errno == EACCES || errno == EPERM)
        return ResponseBuilder::buildErrorResponse(403, serverConfig);
    // Data race gibi sebeplerle dosya artık yoksa istemciye 404 bildirilir.
    else if (errno == ENOENT)
        return ResponseBuilder::buildErrorResponse(404, serverConfig);
    // Yukarıdakiler dışındaki işletim sistemi hataları sunucu iç hata sınıfına girer.
    else
        return ResponseBuilder::buildErrorResponse(500, serverConfig);

    return response;
}