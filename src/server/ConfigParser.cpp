
#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <set>

static std::string stripComments(const std::string &conf)
{
    std::string result;
    bool in_quote = false;
    char quote = '\0';

    for (size_t i = 0; i < conf.length(); i++)
    {
        char c = conf[i];

        if (!in_quote && c == '#')
        {
            while (i < conf.length() && conf[i] != '\n')
                i++;
            if (i < conf.length() && conf[i] == '\n')
                result += '\n';
            continue;
        }
        if (in_quote)
        {
            if (c == quote)
            {
                in_quote = false;
                quote = '\0';
            }
            result += c;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            in_quote = true;
            quote = c;
            result += c;
        }
        else
            result += c;
    }
    return result;
}

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

    std::string all_conf = stripComments(buffer.str());
    size_t i = 0;

    while (i < all_conf.length())
    {
        size_t server_pos = all_conf.find("server", i);
        if (server_pos == std::string::npos)
            break;

        if (server_pos > 0 && !std::isspace(static_cast<unsigned char>(all_conf[server_pos - 1])))
        {
            i = server_pos + 6;
            continue;
        }

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

    std::set<std::pair<std::string, int> > global_listens;
    for (size_t s = 0; s < _servers.size(); ++s)
    {
        const std::set<std::pair<std::string, int> > &listens = _servers[s].listens;
        for (std::set<std::pair<std::string, int> >::const_iterator it = listens.begin(); it != listens.end(); ++it)
        {
            if (!global_listens.insert(*it).second)
            {
                std::stringstream ss;
                ss << "Config parse error: duplicate listen " << it->first << ":" << it->second << " across server blocks";
                throw std::invalid_argument(ss.str());
            }
        }
    }
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
