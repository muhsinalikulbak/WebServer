// FdUtils.hpp
#ifndef FDUTILS_HPP
#define FDUTILS_HPP

namespace FdUtils
{
    void setNonBlocking(int fd);
    void setCloseOnExec(int fd);
    void setTcpNodelay(int fd);
    void setReuseAddress(int fd);
}

#endif