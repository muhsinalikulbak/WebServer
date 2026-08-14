#include "Client.hpp"

/**** CANONIC FORM ****/

Client::Client()
{
    _clientFd = -1;
    _lastActivity = std::time(NULL);
    _clientState = CLOSING;
    _serverConfig = NULL;
    _activeCgi = NULL;
}

Client::Client(int fd)
{
    _clientFd = fd;
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


std::time_t         Client::getLastActivity() const { return _lastActivity; }

ClientState         Client::getClientState() const { return _clientState; }

void                Client::setLastActivity(std::time_t time) { _lastActivity = time; }

void                Client::setClientState(ClientState state) { _clientState = state; }

int                 Client::getFd() const { return _clientFd; } // Override

HandlerType         Client::getType() const { return HANDLER_CLIENT; } // Override

void                Client::setServerConfig(const ServerConfig* config) {_serverConfig = config; }

bool                Client::isBadRequest() { return _parser.hasError(); }


/**** READ WRITE / HELPER FUNCTIONS ****/

StreamState Client::processParserState(RequestParser::State state)
{
    if (state == RequestParser::ERROR)
        return REQUEST_ERROR;
    else if (state == RequestParser::COMPLETE)
        return TRANSFER_COMPLETE;
    return TRANSFER_INCOMPLETE;
}

StreamState Client::receiveData()
{
    char buffer[4096];
    int byte = recv(_clientFd, buffer, 4096, 0);

    if (byte == -1) 
    { 
        perror("Recv() error"); 
        return TRANSFER_ERROR; 
    }

    if (byte == 0)
    {
        // Client bağlantıyı kapattı (EOF), TCP FIN paketi gönderdi
        // Bir client tek bir istek gönderip kendini kapatırsa totalda 4 işlem olur
        // Kendini kapatması demek en son TCP'nin Finish paket göndermersi demektir.
        // 1- Bağlantı İsteği alma recv, 2- Request isteği alma recv 3- Response dönme send, 
        // 4- TCP fin paketi (kapanma) isteği recv
        return PEER_CLOSED;
    }

    RequestParser::State state = _parser.feed(buffer, byte);
    return processParserState(state);
}

StreamState Client::drainBuffer()
{
    RequestParser::State state = _parser.feed(NULL, 0);  // recv yok, sadece kalan buffer'ı işler
    return processParserState(state);
}

StreamState Client::sendData()
{
    // if (_writeBuffer.empty())
    //     return TRANSFER_COMPLETE;  // Tüm veri gönderildi

    // _writeBuffer içindeki veriyi istemciye gönderiyoruz

    // int byte = send(_clientFd, buffer, buffer.size(), 0);

    int byte = send(_clientFd, "response buffer gelecek", 24, 0);

    if (byte == -1)
    {
        perror("Send() error");
        return TRANSFER_ERROR;  // Sistem hatası
    }
    else if (byte > 0)
    {
        // Gönderdiğimiz kısım kadarını writeBuffer'dan siliyoruz
        // _writeBuffer.erase(0, byte);
    }

    // Eğer buffer tamamen bittiyse (her şey gönderildiyse) TRANSFER_COMPLETE
    // if (_writeBuffer.empty())
    //     return TRANSFER_COMPLETE;  // Tüm veri gönderildi

    return TRANSFER_INCOMPLETE;  // Hala gönderilecek veri var
}


void Client::resetParser() { _parser.reset(); }
