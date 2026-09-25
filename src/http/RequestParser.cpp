#include "RequestParser.hpp"
#include <cstdlib>
#include <cerrno>
#include <cctype>

// Creates a RequestParser with the maxBodySize limit, starting in the REQUEST_LINE state.
RequestParser::RequestParser(size_t maxBodySize): _buffer(), _request(), _maxBodySize(maxBodySize), _maxHeaderCount(100)
{
    _state = REQUEST_LINE;
    _chunkedState = SIZE;
    _contentLength = 0;
    _chunkLength = 0;
    _chunkedTotalBytes = 0;
    _bodyBytesRead = 0;
    _headerCount = 0;
    _isChunked = false;
    _errorCode = 400;
}

// Empty destructor; no additional resource management is required.
RequestParser::~RequestParser()
{
}

// Appends raw data read by recv() to the internal buffer.
void RequestParser::append(const std::string& buffer)
{
    if (!buffer.empty())
        _buffer.append(buffer);
}

// Processes buffered data according to the current state until the request is COMPLETE or ERROR.
RequestParser::State RequestParser::feed()
{
    while (_state != COMPLETE && _state != ERROR)
    {

        if (_state == REQUEST_LINE) 
        {
            std::string line;

            if (!extractLine(line))
                break;

            processRequestLine(line);

            if (_state == ERROR)
                break;
            
            _state = HEADERS;
        }
        else if (_state == HEADERS)
        {
            std::string line;
            
            if (!extractLine(line))
                break;

            if (line.empty())
                checkAfterHeader();
            else
            {
                if (++_headerCount > _maxHeaderCount)
                    setError(431);
                else
                    processHeaderLine(line);
            }
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

// After the headers end, selects the body state based on Transfer-Encoding or Content-Length.
void RequestParser::checkAfterHeader()
{
    if (_request.hasHeader("transfer-encoding") && _request.hasHeader("content-length"))
    {
        setError(400);
        return;
    }

    if (_request.hasHeader("transfer-encoding"))
    {
        if (HttpRequest::toLowerCopy(_request.getHeader("transfer-encoding")) != "chunked")
            setError(501);
        else
        {
            _isChunked = true;
            _state = CHUNKED_BODY;
        }
    }
    else if (_request.hasHeader("content-length"))
    {
        if (checkContentLength(_request.getHeader("content-length"), _contentLength, 10))
        {
            if (_contentLength > _maxBodySize)
                setError(413);
            else
                _state = _contentLength > 0 ? BODY : COMPLETE;
        }
        else
            setError(400);
    }
    else
        _state = COMPLETE;
}

// Extracts the next line terminated by "\r\n" from the buffer and consumes it.
bool RequestParser::extractLine(std::string& line)
{
    std::size_t pos = _buffer.find("\r\n");

    if (pos == std::string::npos)
        return false;

    line = _buffer.substr(0, pos);      
    
    _buffer.erase(0, pos + 2);

    return true;
}

// Parses the request line (method, URI, version) and stores it in the request object.
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
        setError(400);
    }
}

// Parses a header line as "name: value" and adds it to the request object.
void RequestParser::processHeaderLine(const std::string& line)
{
    size_t colonPos = line.find(':');
    
    if (colonPos == std::string::npos)
    {
        setError(400);
        return;
    }

    std::string key = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);

    trimString(key);
    trimString(value);

    if (key.empty())
    {
        setError(400);
        return;
    }

    _request.setHeader(key, value);
}

// Trims whitespace from the beginning and end of the given string.
void RequestParser::trimString(std::string& str)
{
    if (str.empty())
        return;

    const std::string whitespace = " \t\r\n\f\v";
    size_t start = str.find_first_not_of(whitespace);

    if (start == std::string::npos)
    {
        str.clear();
        return;
    }

    size_t end = str.find_last_not_of(whitespace);
    str = str.substr(start, end - start + 1);
}



// Splits the given string into tokens using the delimiter character.
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

// Returns the parser's current state.
RequestParser::State    RequestParser::getState() const { return _state; }

// Returns the HttpRequest being built or already completed.
const HttpRequest&      RequestParser::getRequest() { return _request; }

// Returns true if the state is COMPLETE.
bool                    RequestParser::isComplete() const { return _state == COMPLETE; }

// Returns true if the state is ERROR.
bool                    RequestParser::hasError() const { return _state == ERROR; }

// Resets the parser state for the next keep-alive request, excluding the buffer.
void RequestParser::reset()
{
    _state = REQUEST_LINE;
    _contentLength = 0;
    _bodyBytesRead = 0;
    _chunkLength = 0;
    _chunkedTotalBytes = 0;
    _headerCount = 0;
    _isChunked = false;
    _chunkedState = SIZE;
    _request.clear();
}

// Checks whether the Content-Length/chunk-size value is a valid number (base 10 or 16) and stores it in out.
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
    errno = 0;
    unsigned long result = std::strtoul(value.c_str(), &endptr, base);

    if (*endptr != '\0' || errno == ERANGE)
        return false;

    out = static_cast<size_t>(result);
    return true;
}

// Processes the chunked transfer-encoding body through its size, data, and trailer states using the buffer.
bool RequestParser::chunkedBodyRemaining()
{
    if (_chunkedState == SIZE)
    {
        std::string size;

        if (!extractLine(size))
            return false;
        
        if (checkContentLength(size, _chunkLength, 16))
        {
            _bodyBytesRead = 0;
            _chunkedState = _chunkLength == 0 ? TRAILER : DATA;
        }
        else
            setError(400);
    }
    else if (_chunkedState == DATA)
    {
        size_t remaining = _chunkLength - _bodyBytesRead;
        size_t size = std::min(remaining, _buffer.size());

        _chunkedTotalBytes += size;
        if (_chunkedTotalBytes > _maxBodySize)
        {
            setError(413);
            return false;
        }
        
        _bodyBytesRead += size;
        _request.appendBody(_buffer.substr(0, size));
        _buffer.erase(0, size);

        if (_bodyBytesRead == _chunkLength)
        {
            if (_buffer.size() < 2)
                return false;
            
            if (_buffer[0] != '\r' || _buffer[1] != '\n')
            {
                setError(400);
                return false;
            }
            
            _buffer.erase(0, 2);
            _bodyBytesRead = 0;
            _chunkedState = SIZE;
        }

        return !_buffer.empty();
    }
    else
    {
        std::string trailer;

        if (!extractLine(trailer))
            return false;

        if (trailer.empty())
            _state = COMPLETE;
        else
            setError(400);
    }

    return true;
}


// Reads the remaining portion of a fixed-length body known from Content-Length and appends it to the body.
void RequestParser::bodyRemaining()
{
    size_t remaining = _contentLength - _bodyBytesRead;
    size_t size = std::min(_buffer.size(), remaining);
    _bodyBytesRead += size;

    _request.appendBody(_buffer.substr(0, size));
    _buffer.erase(0, size);
}

// Sets the state to ERROR and stores the given HTTP error code.
void RequestParser::setError(int code) { _state = ERROR; _errorCode = code; }

// Returns the HTTP error code produced during parsing.
int  RequestParser::getErrorCode() const { return _errorCode; }
