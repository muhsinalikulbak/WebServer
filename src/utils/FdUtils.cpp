
#include "FdUtils.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <netinet/in.h>
#include <netinet/tcp.h>

namespace FdUtils
{
    // Sets the given file descriptor to non-blocking mode with O_NONBLOCK.
    void setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1)
            throw std::runtime_error(std::string("fcntl F_GETFL: ") + strerror(errno));

        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error(std::string("fcntl F_SETFL: ") + strerror(errno));
    }

    // Adds the FD_CLOEXEC flag to the given file descriptor so it closes automatically after fork/exec.
    void setCloseOnExec(int fd)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1)
            throw std::runtime_error(std::string("fcntl F_GETFD: ") + strerror(errno));

        if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
            throw std::runtime_error(std::string("fcntl F_SETFD: ") + strerror(errno));
    }

    // Disables Nagle's algorithm (TCP_NODELAY) on the given socket file descriptor.
    void setTcpNodelay(int fd)
    {
		int opt = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1)
		{
			throw std::runtime_error(std::string("Error setsockopt TCP_NODELAY: ") + strerror(errno));
		}
    }

    // Enables SO_REUSEADDR on the given socket file descriptor.
    void setReuseAddress(int fd)
    {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            throw std::runtime_error(std::string("setsockopt SO_REUSEADDR: ") + strerror(errno));
    }

    // Enables SO_REUSEPORT on the given socket file descriptor.
    void setReusePort(int fd)
    {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
            throw std::runtime_error(std::string("setsockopt SO_REUSEPORT: ") + strerror(errno));
    }
}

