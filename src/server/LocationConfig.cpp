#include "LocationConfig.hpp"

// Initializes the Location fields with default values (index.html, autoindex disabled, etc.).
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
    clientMaxBodySize = 0;
    hasClientMaxBodySize = false;
}

// Creates a new object by copying another LocationConfig's fields.
LocationConfig::LocationConfig(const LocationConfig& other)
{
    *this = other;
}

// Copies all fields from another LocationConfig into this object.
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
        clientMaxBodySize = other.clientMaxBodySize;
        hasClientMaxBodySize = other.hasClientMaxBodySize;
    }
    return *this;
}

// Empty destructor; no additional resource management is required.
LocationConfig::~LocationConfig()
{
}
