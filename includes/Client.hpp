#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <unistd.h>
#include <arpa/inet.h>

class Client {
private:
    int         _clientFd;     // accept() fonksiyonundan dönen dosya tanımlayıcı
    std::string _readBuffer;   // İstemciden recv ile parça parça okunan ham istek verisi
    std::string _writeBuffer;  // İleride tarayıcıya göndereceğimiz HTTP Cevabı (Response) burada birikecek

    // İhtiyaca göre client'ın durumunu tutmak isteyebilirsin (Örn: Okuma mı yapıyor, yazma mı?)
    // enum ClientState { READING, WRITING, DONE };

public:
    Client(int fd);            // Constructor: accept'ten dönen fd'yi içeri alacak
    ~Client();                 // Destructor: En kritik yer! close(_clientFd) burada yapılacak.

    int  getFd() const;
    const std::string& getReadBuffer() const;
    void clearReadBuffer();

    // Ağ operasyonları
    int  readData();           // İçerisinde SADECE BİR KERE recv() çağrısı yapacak fonksiyon
    int  sendData();           // İleride yazacağın send() çağrısını yapacak fonksiyon
};

#endif