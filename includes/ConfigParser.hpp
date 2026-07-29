
#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

#include "ServerConfig.hpp"

class ConfigParser
{
private:
    std::string _configFile;
    std::vector<ServerConfig> _servers;
public:
    ConfigParser();
    ConfigParser(std::string path);
    ConfigParser(const ConfigParser& other);
    ConfigParser& operator=(const ConfigParser& other);
    ~ConfigParser();

    const std::vector<ServerConfig>& getServers() const;
};

#endif
