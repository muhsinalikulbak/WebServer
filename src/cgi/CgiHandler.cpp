#include "CgiHandler.hpp"

CgiHandler::CgiHandler() : _stdoutPipeFd(-1), _stdinPipeFd(-1), _pid(-1), _owner(NULL)
{
}

CgiHandler::CgiHandler(int stdoutPipeFd, int stdinPipeFd, pid_t pid, Client* owner)
    : _stdoutPipeFd(stdoutPipeFd), _stdinPipeFd(stdinPipeFd), _pid(pid), _owner(owner)
{
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