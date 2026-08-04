#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include <string>
#include "HttpRequest.hpp"

class HttpRequestParser 
{
public:
    enum State {
        REQUEST_LINE,
        HEADERS,
        BODY,
        CHUNKED_BODY,
        COMPLETE,
        ERROR
    };

private:
    State           _state;
    std::string     _buffer;       // henüz işlenmemiş ham byte'lar
    HttpRequest     _request;      // inşa edilmekte olan request

    size_t          _contentLength;
    size_t          _bodyBytesRead;
    // chunked encoding kullanacaksan ayrı state/counter'lar da gerekecek

    bool extractLine(std::string& line);            // buffer'dan \r\n'e kadar bir satır çeker, tüketir
    void processRequestLine(const std::string& line);
    void processHeaderLine(const std::string& line);
    void trimString(std::string& str);

public:
    HttpRequestParser();
    ~HttpRequestParser();

    // recv() sonrası çağrılır, kalan state'e göre devam eder
    State feed(const char* data, size_t len);

    State       getState() const;
    bool        isComplete() const;
    bool        hasError() const;
    HttpRequest& getRequest();

    void reset(); // keep-alive: bir sonraki request için parser'ı sıfırla
};

#endif