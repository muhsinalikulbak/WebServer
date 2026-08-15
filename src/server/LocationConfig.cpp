#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
{
    path = "";
    root = "";
    index = "index.html";
    autoindex = false;
    returnUrl = "";
    returnCode = 0;
    uploadEnable = false;
    uploadStore = "";
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
        allowedMethods = other.allowedMethods;
        autoindex = other.autoindex;
        returnUrl = other.returnUrl;
        returnCode = other.returnCode;
        uploadEnable = other.uploadEnable;
        uploadStore = other.uploadStore;
        cgiExtension = other.cgiExtension;
    }
    return *this;
}

LocationConfig::~LocationConfig()
{
}
