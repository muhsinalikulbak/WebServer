
#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

ConfigParser::ConfigParser()
{
    this->_configFile = "";
    this->_servers.clear();
}

ConfigParser::ConfigParser(std::string path)
{
    this->_configFile = path;

    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::invalid_argument("Could not open file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string all_conf = buffer.str();
    size_t i = 0;

    while (i < all_conf.length())
    {
        size_t server_pos = all_conf.find("server", i);
        if (server_pos == std::string::npos)
            break;

        size_t start_pos = all_conf.find("{", server_pos);
        if (start_pos == std::string::npos)
            throw std::invalid_argument("'{' not found after 'server': " + path);

        std::string between = all_conf.substr(server_pos + 6, start_pos - (server_pos + 6));
        bool only_whitespace = between.find_first_not_of(" \t\r\n") == std::string::npos;
        if (!only_whitespace)
        {
            i = server_pos + 6;
            continue;
        }

        int depth = 1;
        size_t j = start_pos + 1;
        while (j < all_conf.length() && depth > 0)
        {
            if (all_conf[j] == '{')
                depth++;
            else if (all_conf[j] == '}')
                depth--;
            j++;
        }

        if (depth != 0)
            throw std::invalid_argument("Unmatched '{' found: " + path);

        size_t end_pos = j - 1;
        std::string server_block = all_conf.substr(start_pos, end_pos - start_pos + 1);
        _servers.push_back(ServerConfig(server_block));

        i = end_pos + 1;
    }

    if (_servers.empty())
        throw std::invalid_argument("No server block found in config file: " + path);
}

ConfigParser::ConfigParser(const ConfigParser& other)
{
    *this = other;
}

ConfigParser& ConfigParser::operator=(const ConfigParser& other)
{
    if (this != &other)
    {
        this->_configFile = other._configFile;
        this->_servers = other._servers;
    }
    return *this;
}

ConfigParser::~ConfigParser()
{
}

const std::vector<ServerConfig>& ConfigParser::getServers() const
{
    return _servers;
}
