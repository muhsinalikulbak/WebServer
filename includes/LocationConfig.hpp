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
    std::vector<std::string>            allowedMethods;
    bool                                autoindex;
    std::string                         returnUrl;
    int                                 returnCode;
    bool                                uploadEnable;
    std::string                         uploadStore;
    std::map<std::string, std::string>  cgiExtension;

    LocationConfig();
    LocationConfig(const LocationConfig& other);
    LocationConfig& operator=(const LocationConfig& other);
    ~LocationConfig();
};

#endif
