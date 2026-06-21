#include "Client.hpp"

// Constructor: accept'ten dönen fd'yi içeri alacak

Client::Client(int fd)
{
    _clientFd = fd;
}

// Destructor: En kritik yer! close(_clientFd) burada yapılacak.
Client::~Client()
{
    close(_clientFd);
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

}

// Ağ operasyonları
// İçerisinde SADECE BİR KERE recv() çağrısı yapacak fonksiyon
int  Client::readData()
{
    char buffer[4096];
    int byte = 0;

    byte = recv(_clientFd, buffer, 4096, 0);

    if (byte == -1)
    {
        perror ("Recv");
        return -1; // Throw fırlat
    }
    else if (byte == 0)
    {
        // buffer bitti
        return 0;
    }
    else
        _readBuffer.append(buffer, byte);
}           
int  Client::sendData()
{

}   