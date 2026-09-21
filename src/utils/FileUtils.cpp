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