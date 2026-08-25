#include "HttpResponse.hpp"
#include <sstream>

// Statik status-code -> reason phrase eşlemesi
// Sadece bu projede fiilen üretilecek kodları kapsıyor,
// tam RFC listesi değil — yeni bir kod eklersen buraya satır ekle.
std::string HttpResponse::statusTextFor(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

HttpResponse::HttpResponse()
    : _statusCode(200), _statusText(statusTextFor(200)), _version("HTTP/1.1"), _headers(), _body()
{
}

HttpResponse::~HttpResponse()
{
}

HttpResponse::HttpResponse(const HttpResponse& other)
{
    *this = other;
}

HttpResponse& HttpResponse::operator=(const HttpResponse& other)
{
    if (this != &other)
    {
        _statusCode = other._statusCode;
        _statusText = other._statusText;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
    }
    return *this;
}

// status code set etme
void HttpResponse::setStatus(int code)
{
    _statusCode = code;
    _statusText = statusTextFor(code);
}

void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body)
{
    _body = body;
}

// Burada cgi çıktısı için appendBody yerine 
// Direk cgiStdOutBuffer set edilebilir
void HttpResponse::appendBody(const std::string& data)
{
    _body.append(data);
}

int HttpResponse::getStatus() const
{
    return _statusCode;
}

std::string HttpResponse::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end())
        return it->second;
    return "";
}

bool HttpResponse::hasHeader(const std::string& key) const
{
    return _headers.find(key) != _headers.end();
}

const std::string& HttpResponse::getBody() const
{
    return _body;
}

// serialize() fonksiyonunun görevi, 
// HttpResponse sınıfının içindeki verileri 
// HTTP standartlarına (RFC 7230 vb.) uygun tek bir std::string haline getirmektir.

std::string HttpResponse::serialize() const
{
    std::ostringstream out;

    out << _version << " " << _statusCode << " " << _statusText << "\r\n";

    std::map<std::string, std::string>::const_iterator it;
    for (it = _headers.begin(); it != _headers.end(); ++it)
    {
        // Content-Length'i burada elle eklemiş olabilirsin, aşağıda tekrar eklenmesin diye atla
        if (it->first == "Content-Length")
            continue;
        out << it->first << ": " << it->second << "\r\n";
    }

    // Content-Length'i her zaman body'nin gerçek boyutuna göre otomatik yaz
    out << "Content-Length: " << _body.size() << "\r\n";

    out << "\r\n";
    out << _body;

    return out.str();
}

// ÖRNEK RESPONSE

// HTTP/1.1 200 OK\r\n                   <-- Status Line (Sürüm, Kod, Mesaj)
// Content-Type: text/html\r\n           <-- Header (Başlıklar)
// Server: webserv/1.0\r\n                <-- Header
// Content-Length: 13\r\n                <-- Header (Body boyutu)
// \r\n                                  <-- Boş Satır (Headers ile Body'yi ayırır!)
// Hello, World!                         <-- Body (İçerik)