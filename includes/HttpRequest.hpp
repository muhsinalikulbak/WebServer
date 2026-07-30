#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>

class HttpRequest 
{
private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    bool _isValid;

    void parseRequestLine(const std::string& line);
    void parseHeaderLine(const std::string& line);
    void trimString(std::string& str);

public:
    HttpRequest();
    ~HttpRequest();

    bool                parse(const std::string& rawData);

    const std::string&  getMethod() const;
    const std::string&  getUri() const;
    const std::string&  getVersion() const;
    const std::string&  getBody() const;
    std::string         getHeader(const std::string& key) const;
    bool                isValid() const;
};

#endif