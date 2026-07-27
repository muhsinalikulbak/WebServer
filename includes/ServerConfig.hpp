#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include "LocationConfig.hpp"

struct ServerConfig
{
public:
    // Bir server {} scope'u içinde birden fazla listen olabiliyor
    // Bu durumda pair(ip, port) vector'ü oluşturmak gerek sanırım
    std::string                 host;                 // IP Adresi: "127.0.0.1"
    int                         port;                 // Port: 8080
    std::string                 server_name;          // Sunucu İsmi: "example.com"
    size_t                      client_max_body_size; // Bayt Cinsinden Limit: 10485760 (10M)
    std::map<int, std::string>  error_pages;          // Hata Sayfaları: [404] = "/errors/404.html"
    std::vector<LocationConfig> locations;            // İçindeki Location Blokları

    ServerConfig();
    ~ServerConfig();
};

#endif