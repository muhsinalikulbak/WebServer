#include "RequestParser.hpp"
#include <cstdlib> // strtoul

// HttpRequestParser implementasyonu

RequestParser::RequestParser()
    : _state(REQUEST_LINE), _buffer(), _request(), _contentLength(0), _bodyBytesRead(0), _isChunked(false)
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
        std::string line;

        if (_state == REQUEST_LINE) 
        {
            if (!extractLine(line))
                break;

            // 1. extractLine ile ilk satırı çek
            // 2. processRequestLine çağır
            // 3. state = HEADERS yap
        }
        else if (_state == HEADERS) 
        {
            // 1. Satır satır header'ları oku
            // 2. Boş satır görünce body var mı kontrol et -> state = BODY yap
        }
        else if (_state == BODY) 
        {
            _request.appendBody(data, len);
            _buffer.erase(0);
            // 1. _buffer'daki veriyi appendBody yap
            // 2. Boyut tamamlandıysa state = COMPLETE yap
        }
    }
    return _state;
}

// buffer'dan \r\n'e kadar bir satır çeker, tüketir

bool RequestParser::extractLine(std::string& line)
{
    std::size_t pos = line.find("\r\n");

    if (pos == std::string::npos)
        return false;
    
    line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);

    return true;
}          


void RequestParser::processRequestLine(const std::string& line)
{

}

// void processHeaderLine(const std::string& line);
// void trimString(std::string& str);






// std::string::find(const std::string& str)
// Ne yapar: String içinde aradığın parçanın (örneğin "\r\n" veya boşluk " ") index'ini (konumunu) döner.

// Nerede kullanacaksın: Satır sonunu veya Key: Value arasındaki : karakterini bulmak için. Bulamazsa std::string::npos döner.

// std::string::substr(size_t pos, size_t count)

// Ne yapar: Belirtilen pos index'inden başlayarak count kadar karakteri kesip yeni bir string olarak verir.

// Nerede kullanacaksın: GET /index.html HTTP/1.1 içinden GET kısmını cımbızla çekmek için.

// std::string::erase(size_t pos, size_t count)

// Ne yapar: String'in belirtilen kısmını siler/tüketir.

// Nerede kullanacaksın: _buffer içinden işlediğin satırı silip atmak için.

// B. Format Dönüştürme & Temizlik (Helper'lar İçin)
// std::istringstream (veya std::string::find ile boşluk ayırma)

// Ne yapar: Bir string'i tıpkı cin gibi boşluklara göre kelime kelime okumanı sağlar (<sstream> kütüphanesindedir).

// Nerede kullanacaksın: processRequestLine içinde 3 kelimeyi (GET, /, HTTP/1.1) kolayca ayırmak için.

// std::atoi veya std::strtoul (C++98)

// Ne yapar: "1024" gibi string olan sayıları size_t / int tipine çevirir.

// Nerede kullanacaksın: Header'daki Content-Length: 15 değerindeki "15" string'ini sayıya çevirmek için.

// std::isspace / std::cctype

// Ne yapar: Bir karakterin boşluk, tab (\t), newline (\r, \n) olup olmadığını kontrol eder.

// Nerede kullanacaksın: trimString fonksiyonunda Key: Value ayırırken baştaki ve sondaki gereksiz boşlukları temizlemek için.