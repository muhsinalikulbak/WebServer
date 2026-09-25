#include "CgiHandler.hpp"
#include "Client.hpp"

// Creates a default CgiHandler with empty file descriptors, a zero pid, and a NULL owner.
CgiHandler::CgiHandler() : _stdoutPipeFd(-1), _stdinPipeFd(-1), _pid(-1), _owner(NULL), _startTime(std::time(NULL)), _stdinWriteOffset(0)
{
}

// Creates a CgiHandler with the given pipe file descriptors, pid, and owner, and records its start time.
CgiHandler::CgiHandler(int stdoutPipeFd, int stdinPipeFd, pid_t pid, Client* owner)
    : _stdoutPipeFd(stdoutPipeFd), _stdinPipeFd(stdinPipeFd), _pid(pid), _owner(owner), _startTime(std::time(NULL)), _stdinWriteOffset(0)
{
}

// Closes any remaining open stdout/stdin pipe file descriptors.
CgiHandler::~CgiHandler()
{
    if(_stdoutPipeFd != -1)
        close(_stdoutPipeFd);
    if(_stdinPipeFd != -1)
        close(_stdinPipeFd);
}

// Returns the file descriptor of the pipe used to read CGI output.
int CgiHandler::getStdoutFd() const
{
    return _stdoutPipeFd;
}

// Returns the file descriptor of the pipe used to write the request body to CGI.
int CgiHandler::getStdinFd() const
{
    return _stdinPipeFd;
}

// Returns the stdout pipe as the primary file descriptor for the EpollHandler interface.
int CgiHandler::getFd() const
{
    return _stdoutPipeFd;
}

// Returns this handler's epoll handler type (CGI pipe).
EpollHandler::HandlerType CgiHandler::getType() const
{
    return EpollHandler::HANDLER_CGI_PIPE;
}

// Returns the pid of the running CGI child process.
pid_t CgiHandler::getPid() const
{
    return _pid;
}

// Returns the Client that owns this CGI process.
Client* CgiHandler::getOwner() const
{
    return _owner;
}

// Sets the pid of the CGI child process.
void CgiHandler::setPid(pid_t pid)
{
    _pid = pid;
}

// Sets the Client that owns this CGI process.
void CgiHandler::setOwner(Client* owner)
{
    _owner = owner;
}

// Appends newly read CGI data to the output buffer.
void CgiHandler::appendOutput(const std::string& data)
{
    _cgiOutputBuffer.append(data);
}

// Returns the accumulated raw CGI output buffer.
const std::string& CgiHandler::getOutputBuffer() const
{
    return _cgiOutputBuffer;
}

// Removes the stdin pipe from epoll and closes it.
void CgiHandler::closeStdin(int epollFd)
{
    if (_stdinPipeFd != -1)
    {
        epoll_ctl(epollFd, EPOLL_CTL_DEL, _stdinPipeFd, NULL);
        close(_stdinPipeFd);
        _stdinPipeFd = -1;
    }
}

// Sets the starting offset of the owner's request body that has not yet been written to the script.
void CgiHandler::setStdinOffset(size_t offset)
{
    _stdinWriteOffset = offset;
}

// Returns the start of the owner's request body data that has not yet been written to the script.
const char* CgiHandler::stdinRemainingData() const
{
    if (!_owner)
        return NULL;
    return _owner->getRequest().getBody().data() + _stdinWriteOffset;
}

// Returns the number of remaining request body bytes not yet written to the script.
size_t CgiHandler::stdinRemainingSize() const
{
    if (!_owner)
        return 0;
    return _owner->getRequest().getBody().size() - _stdinWriteOffset;
}

// Advances the stdin write offset by n bytes.
void CgiHandler::consumeStdinBuffer(size_t n)
{
    _stdinWriteOffset += n;
}

// Returns the time when the CGI process started (used for timeout checks).
std::time_t CgiHandler::getStartTime() const
{
    return _startTime;
}