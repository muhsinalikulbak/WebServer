#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "LocationConfig.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

struct ServerConfig {
public:
  std::set<std::pair<std::string, int> > listens; // host & port çiftleri
  std::string server_name;                       // Sunucu İsmi: "example.com"
  size_t client_max_body_size; // Bayt Cinsinden Limit: 10485760 (10M)
  std::map<int, std::string>
      error_pages; // Hata Sayfaları: [404] = "/errors/404.html"
  std::vector<LocationConfig> locations; // İçindeki Location Blokları

  ServerConfig();
  ServerConfig(std::string allConf);
  ServerConfig(const ServerConfig &other);
  ServerConfig &operator=(const ServerConfig &other);
  ~ServerConfig();
};

#endif
