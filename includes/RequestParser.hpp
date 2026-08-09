#ifndef REQUESTPARSER_HPP
#define REQUESTPARSER_HPP

#include <string>
#include <vector>
#include <algorithm>

#include "HttpRequest.hpp"

class RequestParser 
{
public:
    enum State 
    {
        REQUEST_LINE,
        HEADERS,
        BODY,
        CHUNKED_BODY,
        COMPLETE,
        ERROR
    };

    enum ChunkedState
    {
        SIZE,
        DATA,
        TRAILER // 0\r\n bitişi işaret eder
    };

private:
    State           _state;
    std::string     _buffer;       // henüz işlenmemiş ham byte'lar
    HttpRequest     _request;      // inşa edilmekte olan request
    ChunkedState    _chunkedState;

    size_t          _contentLength;
    size_t          _chunkLength;
    size_t          _bodyBytesRead;
    bool            _isChunked;    // Transfer-Encoding: chunked tespiti için (şimdilik sadece işaret)
    // chunked encoding kullanacaksan ayrı state/counter'lar da gerekecek

    bool extractLine(std::string& line);          // buffer'dan \r\n'e kadar bir satır çeker, tüketir
    void processRequestLine(const std::string& line);
    void processHeaderLine(const std::string& line);
    void trimString(std::string& str);
    bool checkContentLength(const std::string& value, size_t& out, int base);
    void bodyRemaining();
    bool chunkedBodyRemaining();
    std::vector<std::string> split(const std::string& str, char delimiter);



public:
    RequestParser();
    ~RequestParser();

    // recv() sonrası çağrılır, kalan state'e göre devam eder
    State           feed(const char* data, size_t len);
    State           getState() const;
    bool            isComplete() const;
    bool            hasError() const;
    HttpRequest&    getRequest();

    void reset(); // keep-alive: bir sonraki request için parser'ı sıfırla
};

#endif


