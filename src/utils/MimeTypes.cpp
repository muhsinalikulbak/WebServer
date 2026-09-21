#include "MimeTypes.hpp"

// Path'in extension'ına bakıp uygun MIME type döner.
// Bilinmeyen extension -> "application/octet-stream" (browser bunu indirir, bozuk render etmez).
std::string MimeTypes::fromPath(const std::string& path)
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