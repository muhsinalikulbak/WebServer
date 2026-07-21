#ifndef LOCATIONCONFIG_HPP
# define LOCATIONCONFIG_HPP

# include <string>
# include <vector>
# include <map>

struct LocationConfig
{
public:
    std::string                         path;            // Rota: "/upload"
    std::string                         root;            // Kök Dizin: "/var/www/uploads"
    std::string                         index;           // Varsayılan Dosya: "index.html"
    std::vector<std::string>            allowed_methods; // İzin Verilen Metotlar: ["GET", "POST"]
    bool                                autoindex;       // Dizin Listeleme: true/false
    std::string                         return_url;      // Redirection Adresi: "https://..."
    int                                 return_code;     // Redirection Kodu: 301, 302
    bool                                upload_enable;   // Upload İzni: true/false
    std::string                         upload_store;    // Yükleme Dizini: "/var/www/uploads/files"
    std::map<std::string, std::string>  cgi_extension;   // CGI Eşleşmesi: [".py"] = "/usr/bin/python3"

    LocationConfig();
    ~LocationConfig();
};

#endif