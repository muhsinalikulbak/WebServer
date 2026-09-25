#ifndef EPOLLHANDLER_HPP
#define EPOLLHANDLER_HPP

class EpollHandler
{
public:
    enum HandlerType { HANDLER_LISTEN, HANDLER_CLIENT, HANDLER_CGI_PIPE };

    // Alt sınıfların temizlenmesi için sanal, boş gövdeli yıkıcı.
    virtual ~EpollHandler() {}

    virtual HandlerType getType() const = 0;
    virtual int         getFd() const = 0;
};

#endif