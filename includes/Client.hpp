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
    TRANSFER_COMPLETE,       // Okuma/Yazma işlemi tamamlandı
    REQUEST_ERROR
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
    std::time_t             _lastActivity;
    ClientState             _clientState;
    const ServerConfig*     _serverConfig;  // bu client hangi server bloğuna ait
    CgiHandler*             _activeCgi;     // NULL ise cgi yok
    RequestParser           _parser;
    
    Client(const Client& other);
    Client& operator=(const Client& other);

    StreamState         processParserState(RequestParser::State state);


public:

    Client(int fd);
    ~Client();
    Client();

    virtual HandlerType getType() const; // OVERRIDE
    virtual int         getFd() const;  // OVERRIDE

    ClientState         getClientState() const;
    std::time_t         getLastActivity() const;
    void                setClientState(ClientState state);
    void                setLastActivity(std::time_t time);
    void                setServerConfig(const ServerConfig* config);
    bool                isBadRequest();
    void                resetParser();


    StreamState         drainBuffer();
    StreamState         receiveData();        // İçerisinde SADECE BİR KERE recv() çağrısı yapacak fonksiyon
    StreamState         sendData();           // Send() çağrısını yapacak fonksiyon
};

#endif
