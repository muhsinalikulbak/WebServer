#include "CgiHandler.hpp"
#include "Client.hpp"

// Varsayılan CgiHandler'ı boş fd/pid ve NULL owner ile oluşturur.
CgiHandler::CgiHandler() : _stdoutPipeFd(-1), _stdinPipeFd(-1), _pid(-1), _owner(NULL), _startTime(std::time(NULL)), _stdinWriteOffset(0)
{
}

// Verilen pipe fd'leri, pid ve owner ile CgiHandler'ı oluşturur; başlangıç zamanını damgalar.
CgiHandler::CgiHandler(int stdoutPipeFd, int stdinPipeFd, pid_t pid, Client* owner)
    : _stdoutPipeFd(stdoutPipeFd), _stdinPipeFd(stdinPipeFd), _pid(pid), _owner(owner), _startTime(std::time(NULL)), _stdinWriteOffset(0)
{
}

// Açık kalan stdout/stdin pipe fd'lerini kapatır.
CgiHandler::~CgiHandler()
{
    if(_stdoutPipeFd != -1)
        close(_stdoutPipeFd);
    if(_stdinPipeFd != -1)
        close(_stdinPipeFd);
}

// CGI çıktısını okuduğumuz pipe'ın fd'sini döner.
int CgiHandler::getStdoutFd() const
{
    return _stdoutPipeFd;
}

// CGI'ya body yazdığımız pipe'ın fd'sini döner.
int CgiHandler::getStdinFd() const
{
    return _stdinPipeFd;
}

// EpollHandler arayüzü için birincil fd olarak stdout pipe'ını döner.
int CgiHandler::getFd() const
{
    return _stdoutPipeFd;
}

// Bu handler'ın epoll handler tipini (CGI pipe) döner.
EpollHandler::HandlerType CgiHandler::getType() const
{
    return EpollHandler::HANDLER_CGI_PIPE;
}

// Çalışan CGI child process'in pid'ini döner.
pid_t CgiHandler::getPid() const
{
    return _pid;
}

// Bu CGI'nin sahibi olan Client'ı döner.
Client* CgiHandler::getOwner() const
{
    return _owner;
}

// CGI child process'in pid'ini ayarlar.
void CgiHandler::setPid(pid_t pid)
{
    _pid = pid;
}

// Bu CGI'nin sahibi olan Client'ı ayarlar.
void CgiHandler::setOwner(Client* owner)
{
    _owner = owner;
}

// CGI'dan okunan yeni veriyi çıktı tampununa ekler.
void CgiHandler::appendOutput(const std::string& data)
{
    _cgiOutputBuffer.append(data);
}

// Biriktirilmiş ham CGI çıktı tampununu döner.
const std::string& CgiHandler::getOutputBuffer() const
{
    return _cgiOutputBuffer;
}

// Stdin pipe'ını epoll'dan çıkarıp kapatır.
void CgiHandler::closeStdin(int epollFd)
{
    if (_stdinPipeFd != -1)
    {
        epoll_ctl(epollFd, EPOLL_CTL_DEL, _stdinPipeFd, NULL);
        close(_stdinPipeFd);
        _stdinPipeFd = -1;
    }
}

// Owner'ın body'sinde script'e henüz yazılmamış kısmın başlangıç ofsetini ayarlar.
void CgiHandler::setStdinOffset(size_t offset)
{
    _stdinWriteOffset = offset;
}

// Owner body'sinde henüz script'e yazılmamış verinin başlangıç adresini döner.
const char* CgiHandler::stdinRemainingData() const
{
    if (!_owner)
        return NULL;
    return _owner->getRequest().getBody().data() + _stdinWriteOffset;
}

// Owner body'sinde henüz script'e yazılmamış kalan bayt sayısını döner.
size_t CgiHandler::stdinRemainingSize() const
{
    if (!_owner)
        return 0;
    return _owner->getRequest().getBody().size() - _stdinWriteOffset;
}

// Stdin yazma ofsetini n bayt ileri alır.
void CgiHandler::consumeStdinBuffer(size_t n)
{
    _stdinWriteOffset += n;
}

// CGI'nin başlatıldığı zamanı döner (timeout kontrolü için kullanılır).
std::time_t CgiHandler::getStartTime() const
{
    return _startTime;
}