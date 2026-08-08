
#include "FdUtils.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <netinet/in.h>
#include <netinet/tcp.h>

namespace FdUtils
{
    void setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1)
            throw std::runtime_error(std::string("fcntl F_GETFL: ") + strerror(errno));

        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error(std::string("fcntl F_SETFL: ") + strerror(errno));
    }

    void setCloseOnExec(int fd)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1)
            throw std::runtime_error(std::string("fcntl F_GETFD: ") + strerror(errno));

        if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
            throw std::runtime_error(std::string("fcntl F_SETFD: ") + strerror(errno));
    }

    void setTcpNodelay(int fd)
    {
		int opt = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) // NAGLE ALGORİTMASINI KAPAT
		{
			throw std::runtime_error(std::string("Error setsockopt TCP_NODELAY: ") + strerror(errno));
		}

    }

    void setReuseAddress(int fd)
    {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            throw std::runtime_error(std::string("setsockopt SO_REUSEADDR: ") + strerror(errno));
    }
}
