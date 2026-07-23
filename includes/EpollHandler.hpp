#ifndef EPOLLHANDLER_HPP
#define EPOLLHANDLER_HPP

class EpollHandler
{

public:

    virtual ~EpollHandler() {}

    virtual bool    isListening() const = 0;
    virtual int     getFd() const = 0;
};

// inline EpollHandler::~EpollHandler() {}

#endif