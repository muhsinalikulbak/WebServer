#include "Client.hpp"
#include "FdUtils.hpp"


Client::Client(int fd, const ServerConfig& config) : _serverConfig(config), _parser(config.maxBodyCeiling())
{
    _clientFd = fd;
    _lastActivity = std::time(NULL);
    _clientState = WAITING_FOR_REQUEST;
    _activeCgi = NULL;
    _writeOffset = 0;
}


Client::~Client()
{
    if (_clientFd != -1)
    {
        close(_clientFd);
    }
}

/**** GETTER SETTER ****/


std::time_t                 Client::getLastActivity() const { return _lastActivity; }

Client::ClientState         Client::getClientState() const { return _clientState; }

void                        Client::setLastActivity(std::time_t time) { _lastActivity = time; }

void                        Client::setClientState(Client::ClientState state) { _clientState = state; }

int                         Client::getFd() const { return _clientFd; } // Override

EpollHandler::HandlerType   Client::getType() const { return EpollHandler::HANDLER_CLIENT; } // Override

const ServerConfig&         Client::getServerConfig() const { return _serverConfig; }

const HttpRequest&          Client::getRequest()  { return _parser.getRequest(); }

int                         Client::getErrorCode() const { return _parser.getErrorCode(); }

void                        Client::setWriteBuffer(const std::string& response) { _writeBuffer = response; _writeOffset = 0; }


void                        Client::setActiveCgi(CgiHandler* cgi) { _activeCgi = cgi; }

CgiHandler*                 Client::getActiveCgi() const { return _activeCgi; }


/**** READ WRITE / HELPER FUNCTIONS ****/

bool                        Client::isBadRequest() const { return _parser.hasError(); }

void                        Client::resetParser() { _parser.reset(); }

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
    char buffer[65536];
    int byte = recv(_clientFd, buffer, 4096, 0);

    if (byte == -1) 
    {
        // EAGAIN/EWOULDBLOCK: Non-blocking socket'te veri henüz hazır değil (read) veya buffer dolu (write)
        // Bu beklenen bir durumdur, socket daha sonra tekrar denenmelidir
        // EINTR: Sistem çağrısı bir signal tarafından kesildi, socket/veri hatası değil
        // epoll loop'u socket ready olduğunda işlemi otomatik tekrarlayacaktır
        if (FdUtils::isTransientIoError(errno))
            return TRANSFER_INCOMPLETE;
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
    _parser.append(std::string(buffer, byte));

    if (_activeCgi)
        return TRANSFER_INCOMPLETE;

    RequestParser::State state = _parser.feed();
    return processParserState(state);
}

Client::StreamState Client::drainBuffer()
{
    RequestParser::State state = _parser.feed();  // recv yok, sadece kalan buffer'ı işler
    return processParserState(state);
}

Client::StreamState Client::sendData()
{

    int byte = send(_clientFd, _writeBuffer.data() + _writeOffset, _writeBuffer.size() - _writeOffset, 0);

    if (byte == -1)
    {
        // EAGAIN/EWOULDBLOCK: Non-blocking socket'te buffer dolu, daha sonra tekrar denenmelidir
        // EINTR: Sistem çağrısı bir signal tarafından kesildi, socket/veri hatası değil
        // EPOLLOUT event'i ile socket yazmaya hazır olduğunda işlem tekrarlanacaktır
        if (FdUtils::isTransientIoError(errno))
            return TRANSFER_INCOMPLETE;
        perror("Send() error");
        return TRANSFER_ERROR;  // Sistem hatası
    }
    else if (byte > 0)
    {
        _writeOffset += byte;
        if (_writeOffset == _writeBuffer.size())
        {
            _writeBuffer.clear();
            _writeOffset = 0;
        }
    }
    else if (byte == 0)
    {

        // Bu koşula ihtiyaç var mı emin olamadım
        // Cgi response oluştuktan sonra activeCgi NULL yapılabilir
        // Yani send ederken bir sorun yaratacağını sanmıyorum.

        if (_activeCgi)
        {
            _activeCgi = NULL; //  Şimdilik böyle kapatıyoruz, içindeki verileri henüz bilmiyorum.
            // Temiz bir şekilde kapatılıp NULL set edilecek.
        }
        return TRANSFER_COMPLETE;
    }

    return TRANSFER_INCOMPLETE;  // Hala gönderilecek veri var
}


