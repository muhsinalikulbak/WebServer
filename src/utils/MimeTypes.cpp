#include "MimeTypes.hpp"

// Path'in uzantısına bakıp uygun MIME type'ı döner; bilinmeyen uzantılarda "text/plain" döner.
std::string MimeTypes::fromPath(const std::string& path)
{
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos)
        return "text/plain";

    std::string ext = path.substr(dotPos + 1);

    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css")                  return "text/css";
    if (ext == "js")                   return "application/javascript";
    if (ext == "json")                 return "application/json";
    if (ext == "txt")                  return "text/plain";
    if (ext == "csv")                  return "text/csv";
    if (ext == "xml")                  return "application/xml";

    return "text/plain";
}