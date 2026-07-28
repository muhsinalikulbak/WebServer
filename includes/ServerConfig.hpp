#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include "LocationConfig.hpp"

struct ServerConfig
{
public:
    std::vector<std::pair<std::string , int> >  listens;              // host & port çiftleri
    std::string                                 server_name;          // Sunucu İsmi: "example.com"
    size_t                                      client_max_body_size; // Bayt Cinsinden Limit: 10485760 (10M)
    std::map<int, std::string>                  error_pages;          // Hata Sayfaları: [404] = "/errors/404.html"
    std::vector<LocationConfig>                 locations;            // İçindeki Location Blokları

    ServerConfig(std::string allConf);
    ~ServerConfig();
};

#endif