
#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include "Config.hpp"

class ConfigParser
{
public:
    ConfigParser();
    ~ConfigParser();

    static Config createMockConfig();
};

#endif