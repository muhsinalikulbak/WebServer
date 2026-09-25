#include "CgiResponseParser.hpp"

#include <sstream>
#include <cstdlib>

// Boş yapıcı; durumsuz sınıf için kullanılmaz.
CgiResponseParser::CgiResponseParser()
{
}

// Kopya yapıcı; durumsuz sınıf için kullanılmaz.
CgiResponseParser::CgiResponseParser(const CgiResponseParser&)
{
}

// Atama operatörü; durumsuz sınıf için kullanılmaz.
CgiResponseParser& CgiResponseParser::operator=(const CgiResponseParser&)
{
    return *this;
}

// Yıkıcı; durumsuz sınıf için kullanılmaz.
CgiResponseParser::~CgiResponseParser()
{
}

// CGI'nin ham çıktısını header/body olarak ayırıp bir HttpResponse'a dönüştürür.
HttpResponse CgiResponseParser::parse(const std::string& rawOutput)
{
    std::string::size_type headerEnd = rawOutput.find("\r\n\r\n");
    std::string::size_type separatorLen = 4;

    if (headerEnd == std::string::npos)
    {
        headerEnd = rawOutput.find("\n\n");
        separatorLen = 2;
    }

    std::string headerBlock;
    std::string body;

    if (headerEnd == std::string::npos)
        body = rawOutput;
    else
    {
        headerBlock = rawOutput.substr(0, headerEnd);
        body = rawOutput.substr(headerEnd + separatorLen);
    }

    HttpResponse response;
    response.setStatus(200);
    response.setHeader("Content-Type", "text/html");

    if (!headerBlock.empty())
    {
        std::istringstream headerStream(headerBlock);
        std::string line;

        while (std::getline(headerStream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (line.empty())
                continue;

            std::string::size_type colonPos = line.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            while (!value.empty() && value[0] == ' ')
                value.erase(0, 1);

            if (key == "Status")
            {
                int code = std::atoi(value.c_str());
                if (code > 0)
                    response.setStatus(code);
            }
            else
                response.setHeader(key, value);
        }
    }

    response.setBody(body);
    return response;
}
