#include "CgiHandler.hpp"
#include "Client.hpp"

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

void CgiHandler::closeStdin(int epollFd)
{
    if (_stdinPipeFd != -1)
    {
        // close() kernel'i epoll'dan implicit olarak düşürür ama bu, epoll_wait()'in
        // kullanıcı alanına kopyaladığı ve henüz işlenmemiş anlık bir stdin event'i ile
        // yarışabilir. Açık DEL bu yarışı kapatır; hata olursa yut - fd zaten kapanıyor
        // olabilir (ENOENT ise zaten kayıtlı değildir).
        epoll_ctl(epollFd, EPOLL_CTL_DEL, _stdinPipeFd, NULL);
        close(_stdinPipeFd);
        _stdinPipeFd = -1;
    }
}

void CgiHandler::setStdinOffset(size_t offset)
{
    _stdinWriteOffset = offset;
}

const char* CgiHandler::stdinRemainingData() const
{
    if (!_owner)
        return NULL;
    return _owner->getRequest().getBody().data() + _stdinWriteOffset;
}

size_t CgiHandler::stdinRemainingSize() const
{
    if (!_owner)
        return 0;
    return _owner->getRequest().getBody().size() - _stdinWriteOffset;
}

void CgiHandler::consumeStdinBuffer(size_t n)
{
    _stdinWriteOffset += n;
    const std::string& body = _owner->getRequest().getBody();
    if (_stdinWriteOffset == body.size())
        _stdinWriteOffset = 0;
}

std::time_t CgiHandler::getStartTime() const
{
    return _startTime;
}