#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>

#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

class Router
{
public:
    static const LocationConfig* match(const std::string& path, const ServerConfig& config);

private:
    static bool matchesLocationPath(const std::string& path, const std::string& locationPath);
};

#endif
