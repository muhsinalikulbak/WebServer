
#include "Config.hpp"

Config::Config()
{
}

Config::~Config()
{
}

void Config::addServer(const ServerConfig& server)
{
    _servers.push_back(server);
}

const std::vector<ServerConfig>& Config::getServers() const
{
    return _servers;
}

const ServerConfig* Config::findServer(const std::string& host, int port) const
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port == port && (_servers[i].host == host || _servers[i].host == "0.0.0.0"))
        {
            return &_servers[i];
        }
    }
    if (!_servers.empty())
    {
        return &_servers[0];
    }
    return NULL;
}
