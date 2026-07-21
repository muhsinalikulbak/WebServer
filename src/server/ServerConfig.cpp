#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
{
    host = "0.0.0.0";
    port = 8080;
    server_name = "";
    client_max_body_size = 1048576; // Varsayılan 1MB
}

ServerConfig::~ServerConfig()
{
}