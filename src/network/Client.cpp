#include "Client.hpp"


Client::Client(int fd, const ServerConfig& config) : _tempBuffer(), _serverConfig(config), _parser(config.clientMaxBodySize)
{
    _clientFd = fd;
    _lastActivity = std::time(NULL);
    _clientState = WAITING_FOR_REQUEST;
    _activeCgi = NULL;
    _tempBuffer = "";
}


Client::~Client()
{
    if (_clientFd != -1)
    {
        close(_clientFd);
    }
}

/**** GETTER SETTER ****/


std::time_t               Client::getLastActivity() const { return _lastActivity; }

Client::ClientState       Client::getClientState() const { return _clientState; }

void                      Client::setLastActivity(std::time_t time) { _lastActivity = time; }

void                      Client::setClientState(Client::ClientState state) { _clientState = state; }

int                       Client::getFd() const { return _clientFd; } // Override

EpollHandler::HandlerType Client::getType() const { return EpollHandler::HANDLER_CLIENT; } // Override

const ServerConfig&       Client::getServerConfig() const { return _serverConfig; }

bool                      Client::isBadRequest() const { return _parser.hasError(); }


/**** READ WRITE / HELPER FUNCTIONS ****/

Client::StreamState Client::processParserState(RequestParser::State state)
{
    if (state == RequestParser::ERROR)
        return REQUEST_ERROR;
    else if (state == RequestParser::COMPLETE)
        return TRANSFER_COMPLETE;
    return TRANSFER_INCOMPLETE;
}

Client::StreamState Client::receiveData()
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
    _tempBuffer.append(buffer, byte);
    
    if (_activeCgi)
        return TRANSFER_INCOMPLETE;
    
    RequestParser::State state = _parser.feed(_tempBuffer);
    _tempBuffer.clear();

    return processParserState(state);
}

Client::StreamState Client::drainBuffer()
{
    RequestParser::State state = _parser.feed(_tempBuffer);  // recv yok, sadece kalan buffer'ı işler
    return processParserState(state);
}

Client::StreamState Client::sendData()
{

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
    else 
    {
        if (_activeCgi)
        {
            _activeCgi = NULL; //  Şimdilik böyle kapatıyoruz, içindeki verileri henüz bilmiyorum.
            // Temiz bir şekilde kapatılıp NULL set edilecek.
        }
        return TRANSFER_COMPLETE;
    }

    return TRANSFER_INCOMPLETE;  // Hala gönderilecek veri var
}


void    Client::resetParser() { _parser.reset(); }
