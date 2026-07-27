#include "ServerConfig.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

static std::vector<std::string> tokenizeConfig(const std::string& conf)
{
    std::vector<std::string> tokens;
    std::string current;
    bool in_quote = false;
    char quote = '\0';

    for (size_t i = 0; i < conf.length(); i++)
    {
        char c = conf[i];

        if (!in_quote && c == '#')
        {
            while (i < conf.length() && conf[i] != '\n')
                i++;
            continue;
        }
        if (in_quote)
        {
            if (c == quote)
            {
                tokens.push_back(current);
                current.clear();
                in_quote = false;
                quote = '\0';
            }
            else
                current += c;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            in_quote = true;
            quote = c;
        }
        else if (std::isspace(static_cast<unsigned char>(c)))
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
        }
        else if (c == '{' || c == '}' || c == ';')
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, c));
        }
        else
            current += c;
    }
    if (in_quote)
        throw std::runtime_error("Config parse error: kapanmamis tirnak");
    if (!current.empty())
        tokens.push_back(current);
    return tokens;
}

static int parsePositiveInt(const std::string& token, const std::string& field)
{
    char* end = NULL;
    long value = std::strtol(token.c_str(), &end, 10);

    if (*end != '\0' || value < 0 || value > 65535)
        throw std::runtime_error("Config parse error: gecersiz " + field + ": " + token);
    return static_cast<int>(value);
}

static size_t parseBodySize(const std::string& token)
{
    if (token.empty())
        throw std::runtime_error("Config parse error: bos client_max_body_size");

    size_t multiplier = 1;
    std::string number = token;
    char suffix = token[token.length() - 1];

    if (suffix == 'K' || suffix == 'k' || suffix == 'M' || suffix == 'm' || suffix == 'G' || suffix == 'g')
    {
        number = token.substr(0, token.length() - 1);
        if (suffix == 'K' || suffix == 'k')
            multiplier = 1024;
        else if (suffix == 'M' || suffix == 'm')
            multiplier = 1024 * 1024;
        else
            multiplier = 1024 * 1024 * 1024;
    }

    char* end = NULL;
    unsigned long value = std::strtoul(number.c_str(), &end, 10);
    if (number.empty() || *end != '\0')
        throw std::runtime_error("Config parse error: gecersiz client_max_body_size: " + token);
    return static_cast<size_t>(value) * multiplier;
}

static std::vector<std::string> readDirective(const std::vector<std::string>& tokens, size_t& i)
{
    std::vector<std::string> directive;

    while (i < tokens.size() && tokens[i] != ";")
    {
        if (tokens[i] == "{" || tokens[i] == "}")
            throw std::runtime_error("Config parse error: directive icinde beklenmeyen token: " + tokens[i]);
        directive.push_back(tokens[i]);
        i++;
    }
    if (i >= tokens.size() || tokens[i] != ";")
        throw std::runtime_error("Config parse error: ';' eksik");
    i++;
    return directive;
}

static std::pair<std::string, int> parseListen(const std::string& value)
{
    size_t colon = value.rfind(':');
    std::string host = "0.0.0.0";
    std::string port_text = value;

    if (colon != std::string::npos)
    {
        host = value.substr(0, colon);
        port_text = value.substr(colon + 1);
    }
    if (host.empty() || port_text.empty())
        throw std::runtime_error("Config parse error: gecersiz listen: " + value);
    return std::make_pair(host, parsePositiveInt(port_text, "port"));
}

static bool parseOnOff(const std::string& value, const std::string& field)
{
    if (value == "on")
        return true;
    if (value == "off")
        return false;
    throw std::runtime_error("Config parse error: " + field + " on/off olmali: " + value);
}

static void applyLocationDirective(LocationConfig& location, const std::vector<std::string>& directive)
{
    if (directive.empty())
        throw std::runtime_error("Config parse error: bos location directive");

    const std::string& key = directive[0];
    if (key == "allow_methods")
    {
        if (directive.size() < 2)
            throw std::runtime_error("Config parse error: allow_methods en az bir method ister");
        location.allowed_methods.assign(directive.begin() + 1, directive.end());
    }
    else if (key == "root")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: root tek deger ister");
        location.root = directive[1];
    }
    else if (key == "index")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: index tek deger ister");
        location.index = directive[1];
    }
    else if (key == "autoindex")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: autoindex tek deger ister");
        location.autoindex = parseOnOff(directive[1], "autoindex");
    }
    else if (key == "return")
    {
        if (directive.size() != 3)
            throw std::runtime_error("Config parse error: return kod ve url ister");
        location.return_code = parsePositiveInt(directive[1], "return code");
        location.return_url = directive[2];
    }
    else if (key == "upload_enable")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: upload_enable tek deger ister");
        location.upload_enable = parseOnOff(directive[1], "upload_enable");
    }
    else if (key == "upload_store")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: upload_store tek deger ister");
        location.upload_store = directive[1];
    }
    else if (key == "cgi_ext")
    {
        if (directive.size() != 3)
            throw std::runtime_error("Config parse error: cgi_ext uzanti ve executable ister");
        location.cgi_extension[directive[1]] = directive[2];
    }
    else
        throw std::runtime_error("Config parse error: bilinmeyen location directive: " + key);
}

static LocationConfig parseLocation(const std::vector<std::string>& tokens, size_t& i)
{
    LocationConfig location;

    if (i + 2 >= tokens.size() || tokens[i] != "location")
        throw std::runtime_error("Config parse error: location bekleniyordu");
    location.path = tokens[i + 1];
    if (tokens[i + 2] != "{")
        throw std::runtime_error("Config parse error: location sonrasi '{' eksik");
    i += 3;

    while (i < tokens.size() && tokens[i] != "}")
    {
        std::vector<std::string> directive = readDirective(tokens, i);
        applyLocationDirective(location, directive);
    }
    if (i >= tokens.size() || tokens[i] != "}")
        throw std::runtime_error("Config parse error: location kapanis '}' eksik");
    i++;
    return location;
}

static void applyServerDirective(ServerConfig& server, const std::vector<std::string>& directive)
{
    if (directive.empty())
        throw std::runtime_error("Config parse error: bos server directive");

    const std::string& key = directive[0];
    if (key == "listen")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: listen tek deger ister");
        server.listens.push_back(parseListen(directive[1]));
    }
    else if (key == "server_name")
    {
        if (directive.size() < 2)
            throw std::runtime_error("Config parse error: server_name deger ister");
        server.server_name = directive[1];
    }
    else if (key == "client_max_body_size")
    {
        if (directive.size() != 2)
            throw std::runtime_error("Config parse error: client_max_body_size tek deger ister");
        server.client_max_body_size = parseBodySize(directive[1]);
    }
    else if (key == "error_page")
    {
        if (directive.size() != 3)
            throw std::runtime_error("Config parse error: error_page kod ve path ister");
        server.error_pages[parsePositiveInt(directive[1], "error_page code")] = directive[2];
    }
    else
        throw std::runtime_error("Config parse error: bilinmeyen server directive: " + key);
}

ServerConfig::ServerConfig(std::string allConf)
{
    listens.clear();
    server_name = "";
    client_max_body_size = 1024 * 1024;
    error_pages.clear();
    locations.clear();

    std::vector<std::string> tokens = tokenizeConfig(allConf);
    size_t i = 0;

    if (tokens.size() < 2 || tokens[i] != "{")
        throw std::runtime_error("Config parse error: server bloğu '{' ile baslamali");
    i++;

    while (i < tokens.size() && tokens[i] != "}")
    {
        if (tokens[i] == "location")
            locations.push_back(parseLocation(tokens, i));
        else
        {
            std::vector<std::string> directive = readDirective(tokens, i);
            applyServerDirective(*this, directive);
        }
    }
    if (i >= tokens.size() || tokens[i] != "}")
        throw std::runtime_error("Config parse error: server kapanis '}' eksik");
    i++;
    if (i != tokens.size())
        throw std::runtime_error("Config parse error: server bloğu sonrasi fazla token");
    if (listens.empty())
        throw std::runtime_error("Config parse error: server icinde listen yok");
}

ServerConfig::~ServerConfig()
{
}
