
#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

ConfigParser::ConfigParser(std::string path)
{
    this->_configFile = path;

    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::runtime_error("Dosya acilamadi: " + path);

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
            throw std::runtime_error("'server' sonrasi '{' bulunamadi: " + path);

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
            throw std::runtime_error("Eslesmeyen '{' bulundu: " + path);

        size_t end_pos = j - 1;
        std::string server_block = all_conf.substr(start_pos, end_pos - start_pos + 1);
        _servers.push_back(ServerConfig(server_block));

        i = end_pos + 1;
    }

    if (_servers.empty())
        throw std::runtime_error("Config dosyasinda server bloğu bulunamadi: " + path);
}

ConfigParser::~ConfigParser()
{
}

const std::vector<ServerConfig>& ConfigParser::getServers() const
{
    return _servers;
}
