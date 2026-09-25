#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "LocationConfig.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

struct ServerConfig 
{
private:
  void init();

public:
  std::set<std::pair<std::string, int> > listens; 

  std::string                 serverName;
  size_t                      clientMaxBodySize;
  std::map<int, std::string>  errorPages;
  std::vector<LocationConfig> locations;

  size_t maxBodyCeiling() const;
  size_t effectiveBodyLimit(const LocationConfig* loc) const;

  ServerConfig();
  ServerConfig(const std::string &allConf);
  ServerConfig(const ServerConfig &other);
  ServerConfig &operator=(const ServerConfig &other);
  ~ServerConfig();
};

#endif
