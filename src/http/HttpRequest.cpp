#include "HttpRequest.hpp"

#include <string>
#include <map>
#include <cctype>

// Verilen string'in tamamen küçük harfe çevrilmiş bir kopyasını döner.
std::string HttpRequest::toLowerCopy(const std::string& s)
{
    std::string out;

    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i)
    {
        out.push_back(static_cast<char>(std::tolower(s[i])));
    }
    return out;
}

// Tüm alanları boş değerlerle başlatan varsayılan HttpRequest oluşturur.
HttpRequest::HttpRequest()
    : _method(), _uri(), _version(), _headers(), _body(), _path(), _queryString()
{
}

// Ek kaynak yönetimi gerekmediği için boş yıkıcı.
HttpRequest::~HttpRequest()
{
}

// HTTP metodunu küçük harfe çevirip saklar.
void HttpRequest::setMethod(const std::string& method)
{
    _method = toLowerCopy(method);
}

// URI'yi path ve query string olarak ayırıp saklar.
void HttpRequest::setUri(const std::string& uri)
{
    size_t qPos = uri.find('?');
    if (qPos != std::string::npos)
    {
        _path = uri.substr(0, qPos);
        _queryString = uri.substr(qPos + 1);
    }
    else
    {
        _path = uri;
        _queryString = "";
    }
    _uri = uri;
}

// HTTP versiyonunu küçük harfe çevirip saklar.
void HttpRequest::setVersion(const std::string& version)
{
    _version = toLowerCopy(version);
}

// Bir header'ı (anahtar küçük harfe çevrilerek) ekler veya günceller.
void HttpRequest::setHeader(const std::string& key, const std::string& value)
{
    _headers[toLowerCopy(key)] = value;
}

// Gelen veri parçasını (chunk) body tamponuna ekler.
void HttpRequest::appendBody(const std::string& data)
{
    _body.append(data);
}

// Keep-alive'da bir sonraki isteği işlemek için önceki alanları sıfırlar.
void HttpRequest::clear()
{
    _method.clear();
    _uri.clear();
    _version.clear();
    _body.clear();
    _headers.clear();
}

// HTTP metodunu döner.
const std::string& HttpRequest::getMethod() const
{
    return _method;
}

// Orijinal (path + query string) URI'yi döner.
const std::string& HttpRequest::getUri() const
{
    return _uri;
}

// HTTP versiyonunu döner.
const std::string& HttpRequest::getVersion() const
{
    return _version;
}

// İstek body'sini döner.
const std::string& HttpRequest::getBody() const
{
    return _body;
}

// Query string kısmını döner.
const std::string&  HttpRequest::getQueryString() const
{
    return _queryString;
}

// Query string olmadan yalnızca path kısmını döner.
const std::string&  HttpRequest::getPath() const
{
    return _path;
}

// Verilen header'ın değerini (case-insensitive) döner; yoksa boş string döner.
std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerCopy(key));

    if (it != _headers.end())
        return it->second;
    
    return std::string();
}

// Tüm header'ları içeren map'i döner.
const std::map<std::string, std::string>& HttpRequest::getHeaders() const
{
    return _headers;
}

// Verilen header'ın (case-insensitive) mevcut olup olmadığını döner.
bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(toLowerCopy(key)) != _headers.end();
}

