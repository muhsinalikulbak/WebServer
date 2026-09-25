
#include "FdUtils.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <netinet/in.h>
#include <netinet/tcp.h>

namespace FdUtils
{
    // Verilen fd'yi O_NONBLOCK ile non-blocking moda alır.
    void setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1)
            throw std::runtime_error(std::string("fcntl F_GETFL: ") + strerror(errno));

        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error(std::string("fcntl F_SETFL: ") + strerror(errno));
    }

    // Verilen fd'ye FD_CLOEXEC bayrağını ekler (fork/exec sonrası otomatik kapansın diye).
    void setCloseOnExec(int fd)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1)
            throw std::runtime_error(std::string("fcntl F_GETFD: ") + strerror(errno));

        if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
            throw std::runtime_error(std::string("fcntl F_SETFD: ") + strerror(errno));
    }

    // Verilen soket fd'sinde Nagle algoritmasını (TCP_NODELAY) kapatır.
    void setTcpNodelay(int fd)
    {
		int opt = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1)
		{
			throw std::runtime_error(std::string("Error setsockopt TCP_NODELAY: ") + strerror(errno));
		}
    }

    // Verilen soket fd'sinde SO_REUSEADDR seçeneğini etkinleştirir.
    void setReuseAddress(int fd)
    {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            throw std::runtime_error(std::string("setsockopt SO_REUSEADDR: ") + strerror(errno));
    }

    // Verilen soket fd'sinde SO_REUSEPORT seçeneğini etkinleştirir.
    void setReusePort(int fd)
    {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
            throw std::runtime_error(std::string("setsockopt SO_REUSEPORT: ") + strerror(errno));
    }
}

