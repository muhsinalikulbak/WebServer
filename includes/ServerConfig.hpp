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
  
  // host & port çiftleri
  // Bu set içerisindeki her ip:port aşağıdaki bilgileri baz alacaklar.
  // Yani her ip:port config verilerine kendi içerisinde sahiptir.

  std::string                 serverName;                        // Sunucu İsmi: "example.com"
  size_t                      clientMaxBodySize;                // Bayt Cinsinden Limit: 10485760 (10M)
  std::map<int, std::string>  errorPages;                      // Hata Sayfaları: [404] = "/errors/404.html"
  std::vector<LocationConfig> locations;                      // İçindeki Location Blokları

  // server limiti ve tüm location override'larının en büyüğü.
  // RequestParser request'i routing'den ÖNCE okur, hangi location'a gideceğini bilmez;
  // bu yüzden en gevşek limit tavan olarak kullanılır. Böylece büyük limitli location'a
  // giden body parser'da yanlışlıkla 413 almaz.
  size_t maxBodyCeiling() const;
  // Bir location için geçerli limit: location override verdiyse o değer, yoksa server limiti.
  size_t effectiveBodyLimit(const LocationConfig* loc) const;

  ServerConfig();
  ServerConfig(const std::string &allConf);
  ServerConfig(const ServerConfig &other);
  ServerConfig &operator=(const ServerConfig &other);
  ~ServerConfig();
};

#endif
