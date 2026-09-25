#ifndef CGI_RESPONSE_PARSER_HPP
#define CGI_RESPONSE_PARSER_HPP

#include <string>
#include "HttpResponse.hpp"

class CgiResponseParser
{
public:
    static HttpResponse parse(const std::string& rawOutput);

private:
    CgiResponseParser();
    CgiResponseParser(const CgiResponseParser&);
    CgiResponseParser& operator=(const CgiResponseParser&);
    ~CgiResponseParser();
};

#endif
