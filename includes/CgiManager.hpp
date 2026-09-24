#ifndef CGI_MANAGER_HPP
#define CGI_MANAGER_HPP

#include "CgiHandler.hpp"
#include "EpollHandler.hpp"
#include "HttpResponse.hpp"
#include <string>
#include <set>
#include <vector>
#include <ctime>
#include <sys/types.h>

class Client;

// CgiManager: CGI yaşam döngüsünü yöneten sınıf.
// Server'dan CGI mantığını ayırmak için oluşturuldu.
// Epoll işlemlerini kendisi yapar, _liveHandlers referansı üzerinden
// stale-event kontrolü için Server ile set paylaşır.

class CgiManager
{
private:
    std::set<CgiHandler*>           _cgiHandlers;
    std::vector<CgiHandler*>        _pendingDeletion;
    int&                            _epollFd;
    std::set<EpollHandler*>&        _liveHandlers;
    std::vector<pid_t>              _pendingReap;

    CgiManager(const CgiManager& other);
    CgiManager& operator=(const CgiManager& other);

    void    reapCgiProcess(CgiHandler* handler);
    void    registerCgiStdinWrite(CgiHandler* cgiHandler);

public:
    void    registerHandler(CgiHandler* handler);
    void    unregisterHandler(CgiHandler* handler);
    void    flushPendingDeletions();
    bool    peekCgiExitStatus(CgiHandler* cgiHandler, int& status);
    void    startCgi(Client* client, const std::string& scriptPath, const std::string& interpreterPath);
    void    checkCgiTimeouts(std::time_t now);
    void    finishCgiResponse(CgiHandler* cgiHandler);
    void    handleCgiReceive(CgiHandler* cgiHandler);
    void    handleCgiSend(CgiHandler* cgiHandler);
    void    reapPendingKills();
    CgiManager(int& epollFd, std::set<EpollHandler*>& liveHandlers);
    ~CgiManager();
};

#endif
