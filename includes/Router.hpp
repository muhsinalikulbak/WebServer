#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>

#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

class Router
{
public:
    // path: query string olmadan yalnızca path kısmı.
    // config: bağlı olunan server bloğu; en uygun LocationConfig burada seçilir.
    static const LocationConfig* match(const std::string& path, const ServerConfig& config);

private:
    // Bir location path'inin, request path'inin prefix'i olup olmadığını ve segment sınırını kontrol eder.
    static bool matchesLocationPath(const std::string& path, const std::string& locationPath);
};

#endif
