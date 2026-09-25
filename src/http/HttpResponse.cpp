#include "HttpResponse.hpp"
#include <sstream>

// Returns the fixed reason phrase for the given HTTP status code.
std::string HttpResponse::statusTextFor(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

// Creates an HttpResponse with 200 OK, HTTP/1.1, and an empty body by default.
HttpResponse::HttpResponse()
    : _statusCode(200), _statusText(statusTextFor(200)), _version("HTTP/1.1"), _headers(), _body()
{
}

// Empty destructor; no additional resource management is required.
HttpResponse::~HttpResponse()
{
}

// Creates a new object by copying another HttpResponse's fields.
HttpResponse::HttpResponse(const HttpResponse& other)
{
    *this = other;
}

// Copies all fields from another HttpResponse into this object.
HttpResponse& HttpResponse::operator=(const HttpResponse& other)
{
    if (this != &other)
    {
        _statusCode = other._statusCode;
        _statusText = other._statusText;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
    }
    return *this;
}

// Sets the status code and its corresponding reason phrase.
void HttpResponse::setStatus(int code)
{
    _statusCode = code;
    _statusText = statusTextFor(code);
}

// Adds or updates a header.
void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}

// Replaces the entire response body.
void HttpResponse::setBody(const std::string& body)
{
    _body = body;
}

// Appends the given data to the end of the current body.
void HttpResponse::appendBody(const std::string& data)
{
    _body.append(data);
}

// Returns the response status code.
int HttpResponse::getStatus() const
{
    return _statusCode;
}

// Returns the value of the given header, or an empty string if absent.
std::string HttpResponse::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end())
        return it->second;
    return "";
}

// Returns whether the given header exists.
bool HttpResponse::hasHeader(const std::string& key) const
{
    return _headers.find(key) != _headers.end();
}

// Returns the response body.
const std::string& HttpResponse::getBody() const
{
    return _body;
}

// Converts the HttpResponse into a complete HTTP/1.1 response string (status line, headers, and body).
std::string HttpResponse::serialize() const
{
    std::ostringstream out;

    out << _version << " " << _statusCode << " " << _statusText << "\r\n";
    out << "Connection: keep-alive\r\n";

    std::map<std::string, std::string>::const_iterator it;
    for (it = _headers.begin(); it != _headers.end(); ++it)
    {
        if (it->first == "Content-Length")
            continue;
        out << it->first << ": " << it->second << "\r\n";
    }

    out << "Content-Length: " << _body.size() << "\r\n";

    out << "\r\n";
    out << _body;

    return out.str();
}