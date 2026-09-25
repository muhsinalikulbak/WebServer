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
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::string _path;
    std::string _queryString;


public:
    HttpRequest();
    ~HttpRequest();

    void setMethod(const std::string& method);
    void setUri(const std::string& uri);
    void setVersion(const std::string& version);
    void setHeader(const std::string& key, const std::string& value);
    void appendBody(const std::string& data);
    void clear();
    static std::string toLowerCopy(const std::string& s);


    const std::string&  getMethod() const;
    const std::string&  getUri() const;
    const std::string&  getVersion() const;
    const std::string&  getBody() const;
    const std::string&  getQueryString() const;
    const std::string&  getPath() const;
    std::string         getHeader(const std::string& key) const;
    bool                hasHeader(const std::string& key) const;
    const std::map<std::string, std::string>& getHeaders() const;
};

#endif