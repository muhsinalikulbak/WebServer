#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest 
{
private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::string _contentType;
    std::map<std::string, std::string> _headers;
    std::string _body;

public:
    HttpRequest();
    ~HttpRequest();

    // Parser tarafından doldurulur
    void setMethod(const std::string& method);
    void setUri(const std::string& uri);
    void setVersion(const std::string& version);
    void setHeader(const std::string& key, const std::string& value);
    void appendBody(const std::string& data);
    void clear(); // keep-alive'da bir sonraki request için resetlemek adına

    // Handler/CGI tarafı okur
    const std::string&  getMethod() const;
    const std::string&  getUri() const;
    const std::string&  getVersion() const;
    const std::string&  getBody() const;
    const std::string&  getContentType() const;
    std::string         getHeader(const std::string& key) const;
    bool                hasHeader(const std::string& key) const;
};

#endif


// GET /index.html HTTP/1.1              <-- 1. Satır: Method, URI, Version
// Host: localhost:8080                 <--|
// User-Agent: Mozilla/5.0              <--|  İŞTE BUNLAR "HEADER" (BAŞLIKLAR)
// Content-Type: application/json       <--|  Key: Value şeklinde meta bilgilerdir.
// Content-Length: 15                   <--|

// {"name": "Ali"}                       <-- En alttaki kısım: BODY (Gövde)