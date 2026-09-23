#include "CgiHandler.hpp"

CgiHandler::CgiHandler() : _stdoutPipeFd(-1), _stdinPipeFd(-1), _pid(-1), _owner(NULL), _startTime(std::time(NULL)), _stdinWriteOffset(0)
{
}

CgiHandler::CgiHandler(int stdoutPipeFd, int stdinPipeFd, pid_t pid, Client* owner)
    : _stdoutPipeFd(stdoutPipeFd), _stdinPipeFd(stdinPipeFd), _pid(pid), _owner(owner), _startTime(std::time(NULL)), _stdinWriteOffset(0)
{
    // _startTime: CGI başlangıç zamanı, her CGI isteği için yeni handler oluşturulduğunda bir kez set edilir
    // Timeout kontrolü için kullanılır (10 saniye)
}

CgiHandler::~CgiHandler()
{
    if(_stdoutPipeFd != -1)
        close(_stdoutPipeFd);
    if(_stdinPipeFd != -1)
        close(_stdinPipeFd);
}

int CgiHandler::getStdoutFd() const
{
    return _stdoutPipeFd;
}

int CgiHandler::getStdinFd() const
{
    return _stdinPipeFd;
}

int CgiHandler::getFd() const
{
    return _stdoutPipeFd;
}

EpollHandler::HandlerType CgiHandler::getType() const
{
    return EpollHandler::HANDLER_CGI_PIPE;
}

pid_t CgiHandler::getPid() const
{
    return _pid;
}

Client* CgiHandler::getOwner() const
{
    return _owner;
}

void CgiHandler::setPid(pid_t pid)
{
    _pid = pid;
}

void CgiHandler::setOwner(Client* owner)
{
    _owner = owner;
}

void CgiHandler::appendOutput(const std::string& data)
{
    _cgiOutputBuffer.append(data);
}

const std::string& CgiHandler::getOutputBuffer() const
{
    return _cgiOutputBuffer;
}

void CgiHandler::closeStdin()
{
    if (_stdinPipeFd != -1)
    {
        close(_stdinPipeFd);
        _stdinPipeFd = -1;
    }
}

void CgiHandler::setStdinBuffer(const std::string& data)
{
    _stdinWriteBuffer = data;
    _stdinWriteOffset = 0;
}

void CgiHandler::setStdinBuffer(const std::string& data, size_t offset)
{
    _stdinWriteBuffer = data;
    _stdinWriteOffset = offset;
}

const char* CgiHandler::stdinRemainingData() const
{
    return _stdinWriteBuffer.data() + _stdinWriteOffset;
}

size_t CgiHandler::stdinRemainingSize() const
{
    return _stdinWriteBuffer.size() - _stdinWriteOffset;
}

void CgiHandler::consumeStdinBuffer(size_t n)
{
    _stdinWriteOffset += n;
    if (_stdinWriteOffset == _stdinWriteBuffer.size())
    {
        _stdinWriteBuffer.clear();
        _stdinWriteOffset = 0;
    }
}

std::time_t CgiHandler::getStartTime() const
{
    return _startTime;
}