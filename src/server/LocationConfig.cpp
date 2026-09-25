#include "LocationConfig.hpp"

// Location için varsayılan değerlerle (index.html, autoindex kapalı vb.) alanları başlatır.
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

// Başka bir LocationConfig'in alanlarını kopyalayarak yeni nesne oluşturur.
LocationConfig::LocationConfig(const LocationConfig& other)
{
    *this = other;
}

// Bu nesneye başka bir LocationConfig'in tüm alanlarını atar.
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

// Ek kaynak yönetimi gerekmediği için boş yıkıcı.
LocationConfig::~LocationConfig()
{
}
