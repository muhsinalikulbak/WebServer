#include "ServerConfig.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

static std::vector<std::string> tokenizeConfig(const std::string &conf)
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
    throw std::invalid_argument("Config parse error: unclosed quote");
  if (!current.empty())
    tokens.push_back(current);
  return tokens;
}

static int parsePositiveInt(const std::string &token,
                            const std::string &field)
{
  char *end = NULL;
  long value = std::strtol(token.c_str(), &end, 10);

  if (*end != '\0' || value < 0 || value > 65535)
    throw std::invalid_argument("Config parse error: invalid " + field + ": " +
                                token);
  return static_cast<int>(value);
}

static size_t parseBodySize(const std::string &token)
{
  if (token.empty())
    throw std::invalid_argument(
        "Config parse error: empty client_max_body_size");

  size_t multiplier = 1;
  std::string number = token;
  char suffix = token[token.length() - 1];

  if (suffix == 'K' || suffix == 'k' || suffix == 'M' || suffix == 'm' ||
      suffix == 'G' || suffix == 'g')
  {
    number = token.substr(0, token.length() - 1);
    if (suffix == 'K' || suffix == 'k')
      multiplier = 1024;
    else if (suffix == 'M' || suffix == 'm')
      multiplier = 1024 * 1024;
    else
      multiplier = 1024 * 1024 * 1024;
  }

  char *end = NULL;
  unsigned long value = std::strtoul(number.c_str(), &end, 10);
  if (number.empty() || *end != '\0')
    throw std::invalid_argument(
        "Config parse error: invalid client_max_body_size: " + token);
  return static_cast<size_t>(value) * multiplier;
}

static std::vector<std::string>
readDirective(const std::vector<std::string> &tokens, size_t &i)
{
  std::vector<std::string> directive;

  while (i < tokens.size() && tokens[i] != ";")
  {
    if (tokens[i] == "{" || tokens[i] == "}")
      throw std::invalid_argument(
          "Config parse error: unexpected token inside directive: " +
          tokens[i]);
    directive.push_back(tokens[i]);
    i++;
  }
  if (i >= tokens.size() || tokens[i] != ";")
    throw std::invalid_argument("Config parse error: missing ';'");
  i++;
  return directive;
}

static std::set<std::pair<std::string, int> > parseListen(const std::string &value)
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
    throw std::invalid_argument("Config parse error: invalid listen: " + value);

  std::set<std::pair<std::string, int> > result_set;
  result_set.insert(std::make_pair(host, parsePositiveInt(port_text, "port")));

  return result_set;
}

static bool parseOnOff(const std::string &value, const std::string &field)
{
  if (value == "on")
    return true;
  if (value == "off")
    return false;
  throw std::invalid_argument("Config parse error: " + field +
                              " must be on/off: " + value);
}

static void applyLocationDirective(LocationConfig &location,
                                   const std::vector<std::string> &directive)
{
  if (directive.empty())
    throw std::invalid_argument("Config parse error: empty location directive");

  const std::string &key = directive[0];
  if (key == "allow_methods")
  {
    if (directive.size() < 2)
      throw std::invalid_argument(
          "Config parse error: allow_methods requires at least one method");
    location.allowed_methods.assign(directive.begin() + 1, directive.end());
  }
  else if (key == "root")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: root expects a single value");
    location.root = directive[1];
  }
  else if (key == "index")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: index expects a single value");
    location.index = directive[1];
  }
  else if (key == "autoindex")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: autoindex expects a single value");
    location.autoindex = parseOnOff(directive[1], "autoindex");
  }
  else if (key == "return")
  {
    if (directive.size() != 3)
      throw std::invalid_argument(
          "Config parse error: return requires code and url");
    location.return_code = parsePositiveInt(directive[1], "return code");
    location.return_url = directive[2];
  }
  else if (key == "upload_enable")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: upload_enable expects a single value");
    location.upload_enable = parseOnOff(directive[1], "upload_enable");
  }
  else if (key == "upload_store")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: upload_store expects a single value");
    location.upload_store = directive[1];
  }
  else if (key == "cgi_ext")
  {
    if (directive.size() != 3)
      throw std::invalid_argument(
          "Config parse error: cgi_ext requires extension and executable");
    location.cgi_extension[directive[1]] = directive[2];
  }
  else
    throw std::invalid_argument(
        "Config parse error: unknown location directive: " + key);
}

static LocationConfig parseLocation(const std::vector<std::string> &tokens,
                                    size_t &i)
{
  LocationConfig location;

  if (i + 2 >= tokens.size() || tokens[i] != "location")
    throw std::invalid_argument("Config parse error: location expected");
  location.path = tokens[i + 1];
  if (tokens[i + 2] != "{")
    throw std::invalid_argument(
        "Config parse error: missing '{' after location");
  i += 3;

  while (i < tokens.size() && tokens[i] != "}")
  {
    std::vector<std::string> directive = readDirective(tokens, i);
    applyLocationDirective(location, directive);
  }
  if (i >= tokens.size() || tokens[i] != "}")
    throw std::invalid_argument(
        "Config parse error: missing closing '}' for location");
  i++;
  return location;
}

static void applyServerDirective(ServerConfig &server, const std::vector<std::string> &directive)
{
  if (directive.empty())
    throw std::invalid_argument("Config parse error: empty server directive");

  const std::string &key = directive[0];
  if (key == "listen")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: listen expects a single value");

    std::set<std::pair<std::string, int> > parsed_set = parseListen(directive[1]);
    for (std::set<std::pair<std::string, int> >::const_iterator it = parsed_set.begin();
         it != parsed_set.end(); ++it)
    {
      if (server.listens.find(*it) != server.listens.end())
      {
        std::stringstream ss;
        ss << "Config parse error: duplicate listen " << it->first << ":" << it->second << " in same server block";
        throw std::invalid_argument(ss.str());
      }
      server.listens.insert(*it);
    }
  }
  else if (key == "server_name")
  {
    if (directive.size() < 2)
      throw std::invalid_argument(
          "Config parse error: server_name expects value");
    server.server_name = directive[1];
  }
  else if (key == "client_max_body_size")
  {
    if (directive.size() != 2)
      throw std::invalid_argument(
          "Config parse error: client_max_body_size expects a single value");
    server.client_max_body_size = parseBodySize(directive[1]);
  }
  else if (key == "error_page")
  {
    if (directive.size() != 3)
      throw std::invalid_argument(
          "Config parse error: error_page requires code and path");
    server.error_pages[parsePositiveInt(directive[1], "error_page code")] =
        directive[2];
  }
  else
    throw std::invalid_argument(
        "Config parse error: unknown server directive: " + key);
}

void ServerConfig::init()
{
  listens.clear();
  server_name = "";
  client_max_body_size = 1024 * 1024;
  error_pages.clear();
  locations.clear();
}

ServerConfig::ServerConfig()
{
  init();
}

ServerConfig::ServerConfig(const std::string &allConf)
{
  init();

  std::vector<std::string> tokens = tokenizeConfig(allConf);
  size_t i = 0;

  if (tokens.size() < 2 || tokens[i] != "{")
    throw std::invalid_argument(
        "Config parse error: server block must start with '{'");
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
    throw std::invalid_argument(
        "Config parse error: missing closing '}' for server");
  i++;
  if (i != tokens.size())
    throw std::invalid_argument(
        "Config parse error: extra tokens after server block");
  if (listens.empty())
    throw std::invalid_argument(
        "Config parse error: no listen directive in server");
}

ServerConfig::ServerConfig(const ServerConfig &other) { *this = other; }

ServerConfig &ServerConfig::operator=(const ServerConfig &other)
{
  if (this != &other)
  {
    listens = other.listens;
    server_name = other.server_name;
    client_max_body_size = other.client_max_body_size;
    error_pages = other.error_pages;
    locations = other.locations;
  }
  return *this;
}

ServerConfig::~ServerConfig() {}
