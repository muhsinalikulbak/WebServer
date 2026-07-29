#ifndef LOCATIONCONFIG_HPP
# define LOCATIONCONFIG_HPP

# include <string>
# include <vector>
# include <map>

struct LocationConfig
{
public:
    std::string                         path;
    std::string                         root;
    std::string                         index;
    std::vector<std::string>            allowed_methods;
    bool                                autoindex;
    std::string                         return_url;
    int                                 return_code;
    bool                                upload_enable;
    std::string                         upload_store;
    std::map<std::string, std::string>  cgi_extension;

    LocationConfig();
    LocationConfig(const LocationConfig& other);
    LocationConfig& operator=(const LocationConfig& other);
    ~LocationConfig();
};

#endif
