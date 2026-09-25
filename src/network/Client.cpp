#include "Client.hpp"

// Verilen fd ve server config ile client'ı WAITING_FOR_REQUEST durumunda oluşturur.
Client::Client(int fd, const ServerConfig& config) : _serverConfig(config), _parser(config.maxBodyCeiling())
{
    _clientFd = fd;
    _lastActivity = std::time(NULL);
    _clientState = WAITING_FOR_REQUEST;
    _activeCgi = NULL;
    _writeOffset = 0;
}

// Client soketini kapatır.
Client::~Client()
{
    if (_clientFd != -1)
    {
        close(_clientFd);
    }
}

// Son aktivite zamanını döner.
std::time_t                 Client::getLastActivity() const { return _lastActivity; }

// Client'ın mevcut durumunu döner.
Client::ClientState         Client::getClientState() const { return _clientState; }

// Son aktivite zamanını günceller.
void                        Client::setLastActivity(std::time_t time) { _lastActivity = time; }

// Client'ın durumunu ayarlar.
void                        Client::setClientState(Client::ClientState state) { _clientState = state; }

// Client soketinin fd'sini döner.
int                         Client::getFd() const { return _clientFd; }

// Bu handler'ın epoll handler tipini (CLIENT) döner.
EpollHandler::HandlerType   Client::getType() const { return EpollHandler::HANDLER_CLIENT; }

// Bu client'ın bağlı olduğu server config'ini döner.
const ServerConfig&         Client::getServerConfig() const { return _serverConfig; }

// Parser tarafından inşa edilen HttpRequest'i döner.
const HttpRequest&          Client::getRequest()  { return _parser.getRequest(); }

// Parser'da oluşan HTTP hata kodunu döner.
int                         Client::getErrorCode() const { return _parser.getErrorCode(); }

// Gönderilecek yanıtı yazma tamponuna koyar ve ofseti sıfırlar.
void                        Client::setWriteBuffer(const std::string& response) { _writeBuffer = response; _writeOffset = 0; }

// Bu client için aktif CGI handler'ını ayarlar.
void                        Client::setActiveCgi(CgiHandler* cgi) { _activeCgi = cgi; }

// Bu client için aktif CGI handler'ını döner.
CgiHandler*                 Client::getActiveCgi() const { return _activeCgi; }

// Parser'da hata oluşup oluşmadığını döner.
bool                        Client::isBadRequest() const { return _parser.hasError(); }

// Keep-alive için parser durumunu sıfırlar.
void                        Client::resetParser() { _parser.reset(); }

// RequestParser state'ini Client::StreamState'e çevirir.
Client::StreamState Client::processParserState(RequestParser::State state)
{
    if (state == RequestParser::ERROR)
        return REQUEST_ERROR;
    else if (state == RequestParser::COMPLETE)
        return TRANSFER_COMPLETE;
    return TRANSFER_INCOMPLETE;
}

// Soketten bir recv() çağrısı yapıp okunan veriyi parser'a besler.
Client::StreamState Client::receiveData()
{
    char buffer[65536];
    int byte = recv(_clientFd, buffer, 4096, 0);

    if (byte == -1)
    {
        perror("Recv() error");
        return TRANSFER_ERROR;
    }

    if (byte == 0)
    {
        return PEER_CLOSED;
    }
    _parser.append(std::string(buffer, byte));

    if (_activeCgi)
        return TRANSFER_INCOMPLETE;

    RequestParser::State state = _parser.feed();
    return processParserState(state);
}

// Yeni recv() yapmadan, parser tamponunda kalan veriyi işler.
Client::StreamState Client::drainBuffer()
{
    RequestParser::State state = _parser.feed();
    return processParserState(state);
}

// Yazma tamponundaki kalan yanıt verisini bir send() çağrısıyla client'a gönderir.
Client::StreamState Client::sendData()
{
    ssize_t byte = send(_clientFd, _writeBuffer.data() + _writeOffset,
                        _writeBuffer.size() - _writeOffset, 0);

    if (byte == -1)
    {
        perror("Recv() error");
        return TRANSFER_ERROR;
    }

    if (byte == 0)
        return TRANSFER_ERROR;

    _writeOffset += static_cast<size_t>(byte);
    if (_writeOffset == _writeBuffer.size())
    {
        _writeBuffer.clear();
        _writeOffset = 0;
        return TRANSFER_COMPLETE;
    }
    return TRANSFER_INCOMPLETE;
}

