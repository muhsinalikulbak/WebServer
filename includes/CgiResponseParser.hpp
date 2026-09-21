#ifndef CGI_RESPONSE_PARSER_HPP
#define CGI_RESPONSE_PARSER_HPP

#include <string>
#include "HttpResponse.hpp"

// CgiResponseParser: CGI script'inin ürettiği ham çıktıyı parse edip
// HttpResponse nesnesine dönüştüren saf fonksiyon sınıfı.
// Epoll veya Client bağımlılığı yok, sadece string parsing.

class CgiResponseParser
{
public:
    // CGI ham çıktısını parse edip HttpResponse döndürür.
    // Header/body ayırma ve "Status:" header işleme yapar.
    // Boş output kontrolü burada yapılmaz, çağıran sorumluluğunda.
    static HttpResponse parse(const std::string& rawOutput);

private:
    CgiResponseParser();
    CgiResponseParser(const CgiResponseParser&);
    CgiResponseParser& operator=(const CgiResponseParser&);
    ~CgiResponseParser();
};

#endif
