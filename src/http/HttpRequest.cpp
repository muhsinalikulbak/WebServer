#include "HttpRequest.hpp"

#include <string>
#include <map>
#include <cctype>

// HttpRequest basit bir data holder'dır: setter/getter/append/clear sağlar.
// RFC 7230 uyarınca header field isimleri case-insensitive'dır; bu yüzden
// getHeader ve hasHeader fonksiyonlarında case-insensitive arama yapıyoruz.

// Dosya-düzeyinde yardımcı: verilen string'in küçük harf kopyasını döner
std::string HttpRequest::toLowerCopy(const std::string& s)
{
    std::string out;

    // C++'DE string StringBuilder gibi dinamik çalışır
    // O yüzden arka plandaki reallocation'ları azalatmak için kapasiteyi elle size()
    // Kadar verip realloc yapmadan yola devam ediyoruz.

    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i)
    {
        out.push_back(static_cast<char>(std::tolower(s[i])));
    }
    return out;
}

// Initializer list (üye ilk değer atama listesi) adı verilen bu kısıma
// İstediğimiz değeri verebiliriz, içini boş bırakırsa empty ("") ile doldurur
// Ve garbage value riski ortadan kalkar.

HttpRequest::HttpRequest()
    : _method(), _uri(), _version(), _headers(), _body(), _path(), _queryString()
{
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::setMethod(const std::string& method)
{
    _method = toLowerCopy(method);
}

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
    _uri = uri;  // orijinal tam URI'yi de saklamak isteyebilirsin (log, response header için)
}

// Burada path cgi'ın hangi path de olduğunu söyler
// Sonraki kısım ise script'in kullanacağı verilerdir.
// O yüzden uri'yi ikiye böleriz, path ve query-string olarak

// /cgi-bin/script.py ? name=ali&age=20 #section1
//   \________________/   \_____________/ \_______/
//          |                    |            |
//      Path (Yol)          Query String   Fragment (Çapa)

void HttpRequest::setVersion(const std::string& version)
{
    _version = toLowerCopy(version);
}


void HttpRequest::setHeader(const std::string& key, const std::string& value)
{
    _headers[toLowerCopy(key)] = value; // aynı key varsa üzerine yazar
}


// Parçaları (chunk) verileri alıp _body'e ekleriz.
// Eğer request tarafında state BODY ya da CHUNKED_BODY ise
// Request bu fonksiyonu çağırarak body'i buradaki buffer'da toplar

void HttpRequest::appendBody(const std::string& data)
{
    _body.append(data);
}


// Request bitip response edildikten sonra 
// Tekrar request işlemek için önceki verileri sıfırlarız.
void HttpRequest::clear()
{
    _method.clear();
    _uri.clear();
    _version.clear();
    _body.clear();
    _headers.clear();
}


const std::string& HttpRequest::getMethod() const
{
    return _method;
}


const std::string& HttpRequest::getUri() const
{
    return _uri;
}


const std::string& HttpRequest::getVersion() const
{
    return _version;
}


const std::string& HttpRequest::getBody() const
{
    return _body;
}

const std::string&  HttpRequest::getQueryString() const
{
    return _queryString;
}
const std::string&  HttpRequest::getPath() const
{
    return _path;
}

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerCopy(key));

    if (it != _headers.end())
        return it->second;
    
    return std::string();
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const
{
    return _headers;
}


// İlgili header var mı ?
// Örneğin content-length var mı diye kontrol edilir.

bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(toLowerCopy(key)) != _headers.end();
}

