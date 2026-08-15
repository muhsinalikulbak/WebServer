
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
    bool inQuote = false;
    char quote = '\0';

    for (size_t i = 0; i < conf.length(); i++)
    {
        char c = conf[i];

        if (!inQuote && c == '#')
        {
            while (i < conf.length() && conf[i] != '\n')
                i++;
            if (i < conf.length() && conf[i] == '\n')
                result += '\n';
            continue;
        }
        if (inQuote)
        {
            if (c == quote)
            {
                inQuote = false;
                quote = '\0';
            }
            result += c;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            inQuote = true;
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

    std::string allConf = stripComments(buffer.str());
    size_t i = 0;

    while (i < allConf.length())
    {
        size_t serverPos = allConf.find("server", i);
        if (serverPos == std::string::npos)
            break;

        if (serverPos > 0 && !std::isspace(static_cast<unsigned char>(allConf[serverPos - 1])))
        {
            i = serverPos + 6;
            continue;
        }

        size_t startPos = allConf.find("{", serverPos);
        if (startPos == std::string::npos)
            throw std::invalid_argument("'{' not found after 'server': " + path);

        std::string between = allConf.substr(serverPos + 6, startPos - (serverPos + 6));
        bool onlyWhitespace = between.find_first_not_of(" \t\r\n") == std::string::npos;
        if (!onlyWhitespace)
        {
            i = serverPos + 6;
            continue;
        }

        int depth = 1;
        size_t j = startPos + 1;
        while (j < allConf.length() && depth > 0)
        {
            if (allConf[j] == '{')
                depth++;
            else if (allConf[j] == '}')
                depth--;
            j++;
        }

        if (depth != 0)
            throw std::invalid_argument("Unmatched '{' found: " + path);

        size_t endPos = j - 1;
        std::string serverBlock = allConf.substr(startPos, endPos - startPos + 1);
        _servers.push_back(ServerConfig(serverBlock));

        i = endPos + 1;
    }

    if (_servers.empty())
        throw std::invalid_argument("No server block found in config file: " + path);

    std::set<std::pair<std::string, int> > globalListens;
    for (size_t s = 0; s < _servers.size(); ++s)
    {
        const std::set<std::pair<std::string, int> > &listens = _servers[s].listens;
        for (std::set<std::pair<std::string, int> >::const_iterator it = listens.begin(); it != listens.end(); ++it)
        {
            if (!globalListens.insert(*it).second)
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
