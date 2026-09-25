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
        TRAILER
    };

private:
    State           _state;
    std::string     _buffer;
    HttpRequest     _request;
    ChunkedState    _chunkedState;

    const   size_t  _maxBodySize;
    const   size_t  _maxHeaderCount;
    size_t          _headerCount;
    size_t          _contentLength;
    size_t          _chunkLength;
    size_t          _chunkedTotalBytes;
    size_t          _bodyBytesRead;
    bool            _isChunked;    
    int             _errorCode;
    

    bool    extractLine(std::string& line);
    void    processRequestLine(const std::string& line);
    void    processHeaderLine(const std::string& line);
    void    trimString(std::string& str);
    bool    checkContentLength(const std::string& value, size_t& out, int base);
    void    bodyRemaining();
    bool    chunkedBodyRemaining();
    void    checkAfterHeader();
    void    setError(int code);

    std::vector<std::string> split(const std::string& str, char delimiter);





public:
    RequestParser(size_t maxBodySize);
    ~RequestParser();

    State               feed();
    void                append(const std::string& buffer);
    State               getState() const;
    bool                isComplete() const;
    bool                hasError() const;
    const HttpRequest&  getRequest();
    int                 getErrorCode() const;

    void reset();
};

#endif