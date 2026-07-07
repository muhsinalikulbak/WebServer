#include <iostream>
#include "Server.hpp"
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    Server s;
    s.init(7676);
    s.run();

}
