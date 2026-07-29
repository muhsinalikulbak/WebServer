#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
{
    path = "";
    root = "";
    index = "index.html";
    autoindex = false;
    return_url = "";
    return_code = 0;
    upload_enable = false;
    upload_store = "";
}

LocationConfig::LocationConfig(const LocationConfig& other)
{
    *this = other;
}

LocationConfig& LocationConfig::operator=(const LocationConfig& other)
{
    if (this != &other)
    {
        path = other.path;
        root = other.root;
        index = other.index;
        allowed_methods = other.allowed_methods;
        autoindex = other.autoindex;
        return_url = other.return_url;
        return_code = other.return_code;
        upload_enable = other.upload_enable;
        upload_store = other.upload_store;
        cgi_extension = other.cgi_extension;
    }
    return *this;
}

LocationConfig::~LocationConfig()
{
}
