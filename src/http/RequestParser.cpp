#include "RequestParser.hpp"
#include <cstdlib>


// HttpRequestParser implementasyonu

RequestParser::RequestParser()
    : _state(REQUEST_LINE), _buffer(), _request(), _contentLength(0), _bodyBytesRead(0), _isChunked(false), _chunkedState(SIZE)
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
            std::string line;
            if (!extractLine(line))
                break;
            
            chunkedBodyRemaining(line);
            
            
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
    _buffer.erase(0); // Burada erase etmeli miyim
    _contentLength = 0;
    _bodyBytesRead = 0;
    _isChunked = false;
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

    for (size_t i = 0; i < value.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(value[i])))
            return false;
    }

    char* endptr;
    unsigned long result = std::strtoul(value.c_str(), &endptr, base);

    if (*endptr != '\0')   // strtoul tamamını tüketmediyse
        return false;

    out = static_cast<size_t>(result);
    return true;
}

void RequestParser::chunkedBodyRemaining(const std::string& line)
{
    if (_chunkedState == SIZE)
    {
        if (checkContentLength(line, _contentLength, 16))
            _chunkedState = DATA;
        else
            _state = ERROR;
    } // abcd\r\nselam  4
    else
    {
        _request.appendBody();
        _chunkedState = SIZE;
    }
}


void RequestParser::bodyRemaining()
{
    size_t remaining = _contentLength - _bodyBytesRead;
    size_t size = std::min(_buffer.size(), remaining);
    _bodyBytesRead += size;

    _request.appendBody(_buffer.substr(0, size));
    _buffer.erase(0, size);
}
