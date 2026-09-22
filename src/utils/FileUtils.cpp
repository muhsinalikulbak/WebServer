#include "FileUtils.hpp"
#include <sys/stat.h>
#include <fstream>
#include <sstream>

// stat() ile path'in diskte var olup olmadığını kontrol eder (dosya ya da dizin fark etmez).
bool FileUtils::pathExists(const std::string& path)
{
    // Aynı path kontrolünü tekrar etmemek için küçük yardımcıdır.
    // Dosya mı dizin mi ayrımını burada yapmaz; yalnızca varlık bilgisini döner.
    // Bu sade ayrım üst katmanda doğru HTTP kararını vermeyi kolaylaştırır.
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

// stat() sonucundaki st_mode alanını S_ISDIR makrosuyla kontrol ederek path'in
// dizin mi olduğunu söyler. stat başarısızsa (yok/erişilemiyor) false döner.
bool FileUtils::isDirectory(const std::string& path)
{
    // Path'in tipini (dizin mi) anlamak için stat bilgisini yorumlar.
    // Route davranışında dosya ve dizinin farklı ele alınması gerektiğinden
    // bu ayrım, 403/autoindex/index dosyası kararları için kritik önemdedir.
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

// Dosya içeriğini binary olarak tek seferde belleğe taşır.
// Binary mod, satır sonu dönüşümü gibi platform etkilerini önleyerek
// gönderilecek içeriğin diskteki haliyle birebir korunmasını sağlar.

// Burada ss << file.rdbuf() her durumda içeriği bloklamadan yazıyor mu ?
bool FileUtils::readFile(const std::string& path, std::string& outContent)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    outContent = ss.str();
    return true;
}

// İki path parçasını tek slash sınırıyla birleştirir: ikisi de slash ile bitip
// başlıyorsa birini siler, hiçbiri değilse araya "/" ekler. Boş girdilerde de
// aynı mantık işler (boş base + rest -> "/" + rest; boş rest -> base aynen kalır).
// Neden: resolveFilePath (root+remainder) ve uploadStore+filename birleştirmelerindeki
// çift-slash düzeltmesi tek noktada toplanır, kopya mantık taşınmaz.
std::string FileUtils::joinPath(const std::string& base, const std::string& rest)
{
    std::string result = base;
    bool baseEndsSlash = !result.empty() && result[result.length() - 1] == '/';
    bool restStartsSlash = !rest.empty() && rest[0] == '/';

    if (baseEndsSlash && restStartsSlash)
        result.erase(result.length() - 1);      // çift slash -> tekine indir
    else if (!baseEndsSlash && !restStartsSlash)
        result += "/";                           // slash yok -> ekle

    return result + rest;
}

// Path'in son '/' işaretinden sonraki parçasını döner; '/' yoksa path'in tamamı.
// Neden: upload filenamei çıkarma mantığı handlePost ve handleDelete'te birebir tekrar
// ediliyordu; farklı amaçlar (stat, join) için tek noktada toplanır.
std::string FileUtils::lastPathSegment(const std::string& path)
{
    size_t slashPos = path.find_last_of('/');
    if (slashPos == std::string::npos)
        return path;
    return path.substr(slashPos + 1);
}

// Path sonu '/' ile bitmiyorsa (veya boşsa) sona '/' ekler.
// Neden: autoindex urlPrefix/diskPrefix ve handleGet dirPath normalizasyonu aynı
// mantığı üç kez tekrar ediyordu; tek noktada toplanır.
std::string FileUtils::withTrailingSlash(const std::string& path)
{
    if (path.empty() || path[path.length() - 1] != '/')
        return path + "/";
    return path;
}
