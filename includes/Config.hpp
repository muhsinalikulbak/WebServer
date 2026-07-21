#ifndef CONFIG_HPP
# define CONFIG_HPP

# include <vector>
# include <string>
# include "ServerConfig.hpp"

class Config
{
private:
    std::vector<ServerConfig> _servers;

public:
    Config();
    ~Config();

    void                                addServer(const ServerConfig& server);
    const std::vector<ServerConfig>&    getServers() const;
    const ServerConfig*                 findServer(const std::string& host, int port) const;
};

#endif