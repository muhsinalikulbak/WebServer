#include "HttpRequest.hpp"

#include <string>
#include <map>
#include <cctype>

// Returns a lowercase copy of the given string.
std::string HttpRequest::toLowerCopy(const std::string& s)
{
    std::string out;

    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i)
    {
        out.push_back(static_cast<char>(std::tolower(s[i])));
    }
    return out;
}

// Creates a default HttpRequest with all fields initialized to empty values.
HttpRequest::HttpRequest()
    : _method(), _uri(), _version(), _headers(), _body(), _path(), _queryString()
{
}

// Empty destructor; no additional resource management is required.
HttpRequest::~HttpRequest()
{
}

// Converts the HTTP method to lowercase and stores it.
void HttpRequest::setMethod(const std::string& method)
{
    _method = toLowerCopy(method);
}

// Splits the URI into a path and query string and stores them.
void HttpRequest::setUri(const std::string& uri)
{
    size_t qPos = uri.find('?');
    if (qPos != std::string::npos)
    {
        _path = uri.substr(0, qPos);
        _queryString = uri.substr(qPos + 1);
    }
    else
    {
        _path = uri;
        _queryString = "";
    }
    _uri = uri;
}

// Converts the HTTP version to lowercase and stores it.
void HttpRequest::setVersion(const std::string& version)
{
    _version = toLowerCopy(version);
}

// Adds or updates a header, converting its key to lowercase.
void HttpRequest::setHeader(const std::string& key, const std::string& value)
{
    _headers[toLowerCopy(key)] = value;
}

// Appends the incoming data chunk to the body buffer.
void HttpRequest::appendBody(const std::string& data)
{
    _body.append(data);
}

// Resets the previous fields to process the next keep-alive request.
void HttpRequest::clear()
{
    _method.clear();
    _uri.clear();
    _version.clear();
    _body.clear();
    _headers.clear();
}

// Returns the HTTP method.
const std::string& HttpRequest::getMethod() const
{
    return _method;
}

// Returns the original URI (path + query string).
const std::string& HttpRequest::getUri() const
{
    return _uri;
}

// Returns the HTTP version.
const std::string& HttpRequest::getVersion() const
{
    return _version;
}

// Returns the request body.
const std::string& HttpRequest::getBody() const
{
    return _body;
}

// Returns the query string.
const std::string&  HttpRequest::getQueryString() const
{
    return _queryString;
}

// Returns only the path, without the query string.
const std::string&  HttpRequest::getPath() const
{
    return _path;
}

// Returns the value of the given header (case-insensitive), or an empty string if it is absent.
std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerCopy(key));

    if (it != _headers.end())
        return it->second;
    
    return std::string();
}

// Returns the map containing all headers.
const std::map<std::string, std::string>& HttpRequest::getHeaders() const
{
    return _headers;
}

// Returns whether the given header exists (case-insensitive).
bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(toLowerCopy(key)) != _headers.end();
}

