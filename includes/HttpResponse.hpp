#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse
{
private:
    int                                 _statusCode;
    std::string                         _statusText;
    std::string                         _version;
    std::map<std::string, std::string>  _headers;
    std::string                         _body;

    static std::string statusTextFor(int code);

public:
    HttpResponse();
    ~HttpResponse();
    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    void setStatus(int code);
    void setStatus(int code, const std::string& text); // özel status text istersen
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void appendBody(const std::string& data);

    int                 getStatus() const;
    const std::string&  getHeader(const std::string& key) const;
    bool                hasHeader(const std::string& key) const;
    const std::string&  getBody() const;

    // "HTTP/1.1 200 OK\r\nContent-Type: ...\r\n\r\n<body>" tam stringini üretir
    // Content-Length otomatik hesaplanır/eklenir, çağrıdan önce ekstra bir şey yapmana gerek yok
    std::string serialize() const;
};

#endif