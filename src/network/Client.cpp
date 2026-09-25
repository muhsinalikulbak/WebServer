#include "Client.hpp"

// Creates a client in the WAITING_FOR_REQUEST state with the given file descriptor and server configuration.
Client::Client(int fd, const ServerConfig& config) : _serverConfig(config), _parser(config.maxBodyCeiling())
{
    _clientFd = fd;
    _lastActivity = std::time(NULL);
    _clientState = WAITING_FOR_REQUEST;
    _activeCgi = NULL;
    _writeOffset = 0;
}

// Closes the client socket.
Client::~Client()
{
    if (_clientFd != -1)
    {
        close(_clientFd);
    }
}

// Returns the time of the last activity.
std::time_t                 Client::getLastActivity() const { return _lastActivity; }

// Returns the client's current state.
Client::ClientState         Client::getClientState() const { return _clientState; }

// Updates the time of the last activity.
void                        Client::setLastActivity(std::time_t time) { _lastActivity = time; }

// Sets the client's state.
void                        Client::setClientState(Client::ClientState state) { _clientState = state; }

// Returns the client socket's file descriptor.
int                         Client::getFd() const { return _clientFd; }

// Returns this handler's epoll handler type (CLIENT).
EpollHandler::HandlerType   Client::getType() const { return EpollHandler::HANDLER_CLIENT; }

// Returns the server configuration associated with this client.
const ServerConfig&         Client::getServerConfig() const { return _serverConfig; }

// Returns the HttpRequest built by the parser.
const HttpRequest&          Client::getRequest()  { return _parser.getRequest(); }

// Returns the HTTP error code produced by the parser.
int                         Client::getErrorCode() const { return _parser.getErrorCode(); }

// Places the response in the write buffer and resets the offset.
void                        Client::setWriteBuffer(const std::string& response) { _writeBuffer = response; _writeOffset = 0; }

// Sets the active CGI handler for this client.
void                        Client::setActiveCgi(CgiHandler* cgi) { _activeCgi = cgi; }

// Returns the active CGI handler for this client.
CgiHandler*                 Client::getActiveCgi() const { return _activeCgi; }

// Returns whether the parser encountered an error.
bool                        Client::isBadRequest() const { return _parser.hasError(); }

// Resets the parser state for keep-alive.
void                        Client::resetParser() { _parser.reset(); }

// Converts the RequestParser state to Client::StreamState.
Client::StreamState Client::processParserState(RequestParser::State state)
{
    if (state == RequestParser::ERROR)
        return REQUEST_ERROR;
    else if (state == RequestParser::COMPLETE)
        return TRANSFER_COMPLETE;
    return TRANSFER_INCOMPLETE;
}

// Reads from the socket with recv() and feeds the data to the parser.
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

// Processes the remaining data in the parser buffer without another recv() call.
Client::StreamState Client::drainBuffer()
{
    RequestParser::State state = _parser.feed();
    return processParserState(state);
}

// Sends the remaining response data in the write buffer to the client with one send() call.
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
