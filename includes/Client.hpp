#ifndef CLIENT_HPP
#define CLIENT_HPP


#include "EpollHandler.hpp"
#include "ServerConfig.hpp"
#include "CgiHandler.hpp"
#include "RequestParser.hpp"

#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <cerrno>
#include <cstring>

enum StreamState
{
    TRANSFER_ERROR,         // Sistem hatası (recv/send işlemi başarısız)
    PEER_CLOSED,            // Uzak taraf bağlantıyı kapattı (EOF)
    TRANSFER_INCOMPLETE,    // Okuma/Yazma işlemi tamamlanmadı
    TRANSFER_COMPLETE       // Okuma/Yazma işlemi tamamlandı
};

enum ClientState
{
    READING_REQUEST,
    PROCESSING_REQUEST,
    SENDING_RESPONSE,
    WAITING_FOR_REQUEST,
    CLOSING
};

class Client : public EpollHandler
{
private:
    int                     _clientFd;
    std::string             _readBuffer;   // İstemciden recv ile parça parça okunan ham istek verisi
    std::string             _writeBuffer;  // İleride tarayıcıya göndereceğimiz HTTP Cevabı (Response) burada birikecek
    std::time_t             _lastActivity;
    ClientState             _clientState;
    const ServerConfig*     _serverConfig;  // bu client hangi server bloğuna ait
    CgiHandler*             _activeCgi;     // NULL ise cgi yok
    RequestParser           _parser;
    
    Client(const Client& other);
    Client& operator=(const Client& other);

public:

    Client(int fd);
    ~Client();
    Client();

    virtual HandlerType getType() const;

    ClientState         getClientState() const;
    void                setClientState(ClientState state);
    std::time_t         getLastActivity() const;
    void                setLastActivity(std::time_t time);
    const std::string&  getReadBuffer() const;
    virtual int         getFd() const;
    void                setServerConfig(const ServerConfig* config);

    
    void                clearReadBuffer();
    void                appendToWriteBuffer(const std::string& responseChunk);

    // Ağ operasyonları
    StreamState         receiveData();        // İçerisinde SADECE BİR KERE recv() çağrısı yapacak fonksiyon
    StreamState         sendData();           // Send() çağrısını yapacak fonksiyon
};

#endif
