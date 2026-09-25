#include "HttpResponse.hpp"
#include <sstream>

// Verilen HTTP status koduna karşılık gelen sabit reason phrase'i döner.
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
        case 409: return "Conflict";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

// Varsayılan olarak 200 OK, HTTP/1.1 ve boş body ile HttpResponse oluşturur.
HttpResponse::HttpResponse()
    : _statusCode(200), _statusText(statusTextFor(200)), _version("HTTP/1.1"), _headers(), _body()
{
}

// Ek kaynak yönetimi gerekmediği için boş yıkıcı.
HttpResponse::~HttpResponse()
{
}

// Başka bir HttpResponse'ın alanlarını kopyalayarak yeni nesne oluşturur.
HttpResponse::HttpResponse(const HttpResponse& other)
{
    *this = other;
}

// Bu nesneye başka bir HttpResponse'ın tüm alanlarını atar.
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

// Status kodunu ve buna karşılık gelen reason phrase'i ayarlar.
void HttpResponse::setStatus(int code)
{
    _statusCode = code;
    _statusText = statusTextFor(code);
}

// Bir header'ı ekler veya günceller.
void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}

// Response body'sini tamamen değiştirir.
void HttpResponse::setBody(const std::string& body)
{
    _body = body;
}

// Verilen veriyi mevcut body'nin sonuna ekler.
void HttpResponse::appendBody(const std::string& data)
{
    _body.append(data);
}

// Response status kodunu döner.
int HttpResponse::getStatus() const
{
    return _statusCode;
}

// Verilen header'ın değerini döner; yoksa boş string döner.
std::string HttpResponse::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end())
        return it->second;
    return "";
}

// Verilen header'ın mevcut olup olmadığını döner.
bool HttpResponse::hasHeader(const std::string& key) const
{
    return _headers.find(key) != _headers.end();
}

// Response body'sini döner.
const std::string& HttpResponse::getBody() const
{
    return _body;
}

// HttpResponse'u tam bir HTTP/1.1 yanıt string'ine (status line + header'lar + body) dönüştürür.
std::string HttpResponse::serialize() const
{
    std::ostringstream out;

    out << _version << " " << _statusCode << " " << _statusText << "\r\n";
    out << "Connection: keep-alive\r\n";

    std::map<std::string, std::string>::const_iterator it;
    for (it = _headers.begin(); it != _headers.end(); ++it)
    {
        if (it->first == "Content-Length")
            continue;
        out << it->first << ": " << it->second << "\r\n";
    }

    out << "Content-Length: " << _body.size() << "\r\n";

    out << "\r\n";
    out << _body;

    return out.str();
}