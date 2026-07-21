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

LocationConfig::~LocationConfig()
{
}