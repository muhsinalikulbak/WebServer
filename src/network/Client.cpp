#include "Client.hpp"

/**** CANONIC FORM ****/

Client::Client()
{
    _clientFd = -1;
    _readBuffer = "";
    _writeBuffer = "";
    _lastActivity = std::time(NULL);
    _clientState = CLOSING;
    _serverConfig = NULL;
    _activeCgi = NULL;
}

Client::Client(int fd)
{
    _clientFd = fd;
    _readBuffer = "";
    _writeBuffer = "";
    _lastActivity = std::time(NULL);
    _clientState = WAITING_FOR_REQUEST;
    _serverConfig = NULL;
    _activeCgi = NULL;
}


Client::~Client()
{
    if (_clientFd != -1)
    {
        close(_clientFd);
    }
}

/**** GETTER SETTER ****/

const std::string&  Client::getReadBuffer() const { return _readBuffer; }

std::time_t         Client::getLastActivity() const { return _lastActivity; }

ClientState         Client::getClientState() const { return _clientState; }

void                Client::setLastActivity(std::time_t time) { _lastActivity = time; }

void                Client::setClientState(ClientState state) { _clientState = state; }

int                 Client::getFd() const { return _clientFd; } // Override

HandlerType         Client::getType() const { return HANDLER_CLIENT; } // Override

void                Client::setServerConfig(const ServerConfig* config) {_serverConfig = config; }



/**** READ WRITE / HELPER FUNCTIONS ****/


void Client::clearReadBuffer()
{
    _readBuffer.clear();
}

StreamState Client::receiveData()
{
    char buffer[4096];
    int byte = 0;

    byte = recv(_clientFd, buffer, 4096, 0);

    if (byte == -1)
    {
        perror("Recv() error");
        return TRANSFER_ERROR;
    }
    else if (byte == 0)
    {
        return PEER_CLOSED;  
        // Client bağlantıyı kapattı (EOF), TCP FIN paketi gönderdi
        // Bir client tek bir istek gönderip kendini kapatırsa totalda 4 işlem olur
        // Kendini kapatması demek en son TCP'nin Finish paket göndermersi demektir.
        // 1- Bağlantı İsteği alma recv, 2- Request isteği alma recv 3- Response dönme send, 4- TCP fin paketi (kapanma) isteği recv   
    }
    else
    {
        _readBuffer.append(buffer, byte);
    }
    
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
        perror("Send() error");
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

void    Client::appendToWriteBuffer(const std::string& responseChunk)
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

