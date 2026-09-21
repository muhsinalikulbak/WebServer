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
    size_t                              clientMaxBodySize;        // location bazlı limit; server limitini o location için ezer.
    bool                                hasClientMaxBodySize;     // direktif verilmiş mi? (subject /post_body maxBody 100 şartı)

    LocationConfig();
    LocationConfig(const LocationConfig& other);
    LocationConfig& operator=(const LocationConfig& other);
    ~LocationConfig();
};

#endif
