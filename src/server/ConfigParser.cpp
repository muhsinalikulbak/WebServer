
#include "ConfigParser.hpp"

ConfigParser::ConfigParser()
{
}

ConfigParser::~ConfigParser()
{
}

Config ConfigParser::createMockConfig()
{
    Config config;

    // --- SERVER 1 (Port 8080) ---
    ServerConfig server1;
    server1.host = "127.0.0.1";
    server1.port = 8080;
    server1.server_name = "example.com";
    server1.client_max_body_size = 10485760; // 10MB

    server1.error_pages[404] = "/errors/404.html";
    server1.error_pages[500] = "/errors/500.html";

    // Location: /
    LocationConfig locRoot;
    locRoot.path = "/";
    locRoot.root = "/var/www/html";
    locRoot.index = "index.html";
    locRoot.allowed_methods.push_back("GET");
    locRoot.allowed_methods.push_back("POST");
    locRoot.autoindex = true;
    server1.locations.push_back(locRoot);

    // Location: /upload
    LocationConfig locUpload;
    locUpload.path = "/upload";
    locUpload.root = "/var/www/uploads";
    locUpload.allowed_methods.push_back("POST");
    locUpload.allowed_methods.push_back("DELETE");
    locUpload.upload_enable = true;
    locUpload.upload_store = "/var/www/uploads/files";
    server1.locations.push_back(locUpload);

    // Location: /cgi-bin
    LocationConfig locCgi;
    locCgi.path = "/cgi-bin";
    locCgi.root = "/var/www/cgi-bin";
    locCgi.allowed_methods.push_back("GET");
    locCgi.allowed_methods.push_back("POST");
    locCgi.cgi_extension[".py"] = "/usr/bin/python3";
    locCgi.cgi_extension[".php"] = "/usr/bin/php-cgi";
    server1.locations.push_back(locCgi);

    config.addServer(server1);

    // --- SERVER 2 (Port 9090) ---
    ServerConfig server2;
    server2.host = "0.0.0.0";
    server2.port = 9090;
    server2.server_name = "test.com";
    server2.client_max_body_size = 2097152; // 2MB

    LocationConfig locTest;
    locTest.path = "/";
    locTest.root = "/var/www/test";
    locTest.index = "default.html";
    locTest.allowed_methods.push_back("GET");
    locTest.autoindex = false;
    server2.locations.push_back(locTest);

    config.addServer(server2);

    return config;
}
