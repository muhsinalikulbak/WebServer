#include "RequestParser.hpp"
#include <cstdlib>


// HttpRequestParser implementasyonu

RequestParser::RequestParser()
    : _state(REQUEST_LINE), _buffer(), _request(), _chunkedState(SIZE),  _contentLength(0),  _chunkLength(0), _bodyBytesRead(0), _isChunked(false)
{
}

RequestParser::~RequestParser()
{
}


RequestParser::State RequestParser::feed(const char* data, size_t len)
{
    _buffer.append(data, len); // Gelen veriyi depola

    while (_state != COMPLETE && _state != ERROR)
    {

        if (_state == REQUEST_LINE) 
        {
            std::string line;

            if (!extractLine(line))
                break;

            processRequestLine(line);
            _state = HEADERS;
        }
        else if (_state == HEADERS)
        {   
            std::string line;
            
            if (!extractLine(line))
                break;

            if (line.empty())
            {
                if (_request.hasHeader("transfer-encoding") && _request.hasHeader("content-length"))
                {
                    _state = ERROR;
                    break;
                }

                if (_request.hasHeader("transfer-encoding") && 
                    _request.getHeader("transfer-encoding") == "chunked")
                {
                    _isChunked = true;
                    _state = CHUNKED_BODY;
                }
                else if (_request.hasHeader("content-length"))
                {
                    if (checkContentLength(_request.getHeader("content-length"), _contentLength, 10))
                        _state = _contentLength > 0 ? BODY : COMPLETE;
                    else
                        _state = ERROR;
                }
                else
                    _state = COMPLETE;
            }
            else
                processHeaderLine(line);
        }
        else if (_state == BODY)
        {
            bodyRemaining();

            if (_bodyBytesRead == _contentLength)
                _state = COMPLETE;
            else
                break;
        }
        else if (_state == CHUNKED_BODY)
        {
            if (!chunkedBodyRemaining())
                break;
        }
    }
    return _state;
}

// buffer'dan \r\n'e kadar bir satır çeker, tüketir

bool RequestParser::extractLine(std::string& line)
{
    std::size_t pos = _buffer.find("\r\n");

    if (pos == std::string::npos)
        return false;

    // İlgili satırı line'a alıyoruz
    line = _buffer.substr(0, pos);      
    
    // Satırı aldıktan sonra buffer'dan o satırı siliyoruz.
    _buffer.erase(0, pos + 2);

    return true;
}


void RequestParser::processRequestLine(const std::string& line)
{
    std::vector<std::string> requestLine = split(line, ' ');
    
    if (requestLine.size() == 3)
    {
        _request.setMethod(requestLine[0]);
        _request.setUri(requestLine[1]);
        _request.setVersion(requestLine[2]);
    }
    else
    {
        _state = ERROR;
    }
}

void RequestParser::processHeaderLine(const std::string& line)
{
    size_t colonPos = line.find(':');
    
    if (colonPos != std::string::npos)
    {
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        trimString(key);
        trimString(value);
    
        _request.setHeader(key, value);
    }
}

// Kontrol et
void RequestParser::trimString(std::string& str)
{
    if (str.empty())
        return;

    size_t start = 0;
    size_t end = str.size() - 1;

    while (str[start] && std::isspace(str[start]))
        start++;
    
    while (str[start] && str[end] && std::isspace(str[end]))
        end--;
    
    str = str.substr(start, end - start + 1);
}




// GET /index.html HTTP/1.1             <-- 1. Satır: Method, URI, Version
// Host: localhost:8080                 <--|
// User-Agent: Mozilla/5.0              <--|  İŞTE BUNLAR "HEADER" (BAŞLIKLAR)
// Content-Type: application/json       <--|  Key: Value şeklinde meta bilgilerdir. // Content type olmalı mı
// Content-Length: 15                   <--|

// {"name": "Ali"}                       <-- En alttaki kısım: BODY (Gövde)



std::vector<std::string> RequestParser::split(const std::string& str, char delimiter) 
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) 
    {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}


RequestParser::State RequestParser::getState() const { return _state; }

HttpRequest&    RequestParser::getRequest() { return _request; }

bool            RequestParser::isComplete() const { return _state == COMPLETE; }

bool            RequestParser::hasError() const { return _state == ERROR; }



void RequestParser::reset()
{
    _state = REQUEST_LINE;
    _contentLength = 0;
    _bodyBytesRead = 0;
    _isChunked = false;
    _chunkedState = SIZE;
    _chunkLength = 0;
    _request.clear();
} 
// keep-alive: bir sonraki request için parser'ı sıfırla



// digit check + endptr kontrolü olmasının sebebi
// stroul "   123" gibi baştaki boşlukları da kabul eder
// aynı zamanda negatif sayıları da unsigned'a çevirir.
// O yüzden digit check + endptr kontrolü yapılır

bool RequestParser::checkContentLength(const std::string& value, size_t& out, int base)
{
    if (value.empty())
        return false;
    char ch;

    for (size_t i = 0; i < value.size(); ++i)
    {
        ch = static_cast<unsigned char>(value[i]);

        if (!std::isxdigit(ch) && base == 16)
            return false;
        
        if (!std::isdigit(ch) && base == 10)
            return false;
    }

    char* endptr;
    unsigned long result = std::strtoul(value.c_str(), &endptr, base);

    if (*endptr != '\0')   // strtoul tamamını tüketmediyse
        return false;

    out = static_cast<size_t>(result);
    return true;
}

bool RequestParser::chunkedBodyRemaining()
{
    if (_chunkedState == SIZE)
    {
        std::string size;

        if (!extractLine(size))
            return false;
        
        if (checkContentLength(size, _chunkLength, 16))
            _chunkedState = _chunkLength == 0 ? TRAILER : DATA;
        else
            _state = ERROR;
    }
    else if (_chunkedState == DATA)
    {
        // 0\r\n

        size_t remaining = _chunkLength - _bodyBytesRead;
        size_t size = std::min(remaining, _buffer.size());
        
        _bodyBytesRead += size;
        _request.appendBody(_buffer.substr(0, size));
        _buffer.erase(0, size);

        if (_bodyBytesRead == _chunkLength)
        {
            if (_buffer.size() < 2)
                return false;
            
            if (_buffer[0] != '\r' || _buffer[1] != '\n')
            {
                _state = ERROR;
                return false;
            }
            
            _buffer.erase(0, 2);
            _chunkedState = SIZE;
        }
    }
    else
    {
        std::string trailer;

        if (!extractLine(trailer))
            return false;
        
            // size kısmında 0\r\n  '0' kısmı silindi ve
            // Burada \r\n kısmı da silinerek complete edildi
        _state = trailer.empty() ? COMPLETE : ERROR;
    }

    return true;
}


void RequestParser::bodyRemaining()
{
    size_t remaining = _contentLength - _bodyBytesRead;
    size_t size = std::min(_buffer.size(), remaining);
    _bodyBytesRead += size;

    _request.appendBody(_buffer.substr(0, size));
    _buffer.erase(0, size);
}
