#include "Client.hpp"

// Constructor: accept'ten dönen fd'yi içeri alacak

Client::Client(int fd)
{
    _clientFd = fd;
    _readBuffer = "";
    _writeBuffer = "";
}

// Destructor: En kritik yer! close(_clientFd) burada yapılacak.
Client::~Client()
{
    if (_clientFd != -1)
    {
        close(_clientFd);
    }
}

int  Client::getFd() const
{
    return _clientFd;
}

const std::string& Client::getReadBuffer() const
{
    return _readBuffer;
}

void Client::clearReadBuffer()
{
    _readBuffer.clear();
}

// Ağ operasyonları
// İçerisinde SADECE BİR KERE recv() çağrısı yapacak fonksiyon
StreamState Client::receiveData()
{
    char buffer[4096];
    int byte = 0;

    byte = recv(_clientFd, buffer, 4096, 0);

    if (byte == -1)
    {
        perror("Recv");
        return TRANSFER_ERROR;  // Sistem hatası
    }
    else if (byte == 0)
    {
        return PEER_CLOSED;  // Client bağlantıyı kapattı (EOF), TCP FIN paketi gönderdi
    }
    else
        _readBuffer.append(buffer, byte);
    
    if (_readBuffer.find("\r\n\r\n") != std::string::npos)
    {
        // Veri okundu ve istek tamamen alındı
        return TRANSFER_COMPLETE;
    }
    // Hala alınacak veri var
    return TRANSFER_INCOMPLETE;
}

StreamState Client::sendData()
    {
        if (_writeBuffer.empty())
            return TRANSFER_COMPLETE;  // Tüm veri gönderildi

        // _writeBuffer içindeki veriyi istemciye gönderiyoruz
        int byte = send(_clientFd, _writeBuffer.c_str(), _writeBuffer.size(), 0);

        if (byte == -1)
        {
            perror("Send");
            return TRANSFER_ERROR;  // Sistem hatası
        }
        else if (byte > 0)
        {
            // Gönderdiğimiz kısım kadarını writeBuffer'dan siliyoruz
            _writeBuffer.erase(0, byte);
        }

        // Eğer buffer tamamen bittiyse (her şey gönderildiyse) TRANSFER_COMPLETE
        if (_writeBuffer.empty())
            return TRANSFER_COMPLETE;  // Tüm veri gönderildi

        return TRANSFER_INCOMPLETE;  // Hala gönderilecek veri var
}

void Client::appendToWriteBuffer(const std::string& responseChunk)
{
    _writeBuffer.append(responseChunk);

    // Burada bir akış söz konusudur. Yani bir response gönderilirken 
    // O esnada bir request gelip response üretilip bu writebuffer'ın kuyruğuna eklenmelidir.
    // Diğer türlü = eşitleme o an gönderilen response'u siler.

    // örneğin merhaba\r\n\r\ response'u varken
    // Sonra selam response'U geldiğinde durum > merhaba\r\n\r\selam\r\n\r\ olur ,
    // Send data 0. indexten veri gönderir merhaba'dan devam eder.

    // Client'a farklı requestlerin response'ları tek bir string üzerinde birleştirilip gönderilmesi sorun değildir.
    // Client tarafında HTTP 1.1 protokolü bunu kendi içinde halleder. 
}
