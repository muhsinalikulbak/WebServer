#include "Server.hpp"
#include "ConfigParser.hpp"
#include <exception>
#include <iostream>

static void printStringVector(const std::vector<std::string>& values)
{
	for (size_t i = 0; i < values.size(); i++)
	{
		if (i != 0)
			std::cout << ", ";
		std::cout << values[i];
	}
}

static void printConfigParser(const ConfigParser& config)
{
	const std::vector<ServerConfig>& servers = config.getServers();

	std::cout << "========== CONFIG PARSER ==========" << std::endl;
	std::cout << "server count: " << servers.size() << std::endl;
	for (size_t i = 0; i < servers.size(); i++)
	{
		const ServerConfig& server = servers[i];

		std::cout << std::endl;
		std::cout << "[server " << i << "]" << std::endl;
		std::cout << "server_name: " << server.server_name << std::endl;
		std::cout << "client_max_body_size: " << server.client_max_body_size << std::endl;

		std::cout << "listens (" << server.listens.size() << "):" << std::endl;
		for (size_t j = 0; j < server.listens.size(); j++)
		{
			std::cout << "  - host: " << server.listens[j].first
					  << ", port: " << server.listens[j].second << std::endl;
		}

		std::cout << "error_pages (" << server.error_pages.size() << "):" << std::endl;
		for (std::map<int, std::string>::const_iterator it = server.error_pages.begin();
			 it != server.error_pages.end(); ++it)
		{
			std::cout << "  - " << it->first << " -> " << it->second << std::endl;
		}

		std::cout << "locations (" << server.locations.size() << "):" << std::endl;
		for (size_t j = 0; j < server.locations.size(); j++)
		{
			const LocationConfig& location = server.locations[j];

			std::cout << "  [location " << j << "]" << std::endl;
			std::cout << "    path: " << location.path << std::endl;
			std::cout << "    root: " << location.root << std::endl;
			std::cout << "    index: " << location.index << std::endl;
			std::cout << "    allowed_methods: ";
			printStringVector(location.allowed_methods);
			std::cout << std::endl;
			std::cout << "    autoindex: " << (location.autoindex ? "on" : "off") << std::endl;
			std::cout << "    return_code: " << location.return_code << std::endl;
			std::cout << "    return_url: " << location.return_url << std::endl;
			std::cout << "    upload_enable: " << (location.upload_enable ? "on" : "off") << std::endl;
			std::cout << "    upload_store: " << location.upload_store << std::endl;
			std::cout << "    cgi_extension (" << location.cgi_extension.size() << "):" << std::endl;
			for (std::map<std::string, std::string>::const_iterator it = location.cgi_extension.begin();
				 it != location.cgi_extension.end(); ++it)
			{
				std::cout << "      - " << it->first << " -> " << it->second << std::endl;
			}
		}
	}
	std::cout << "===================================" << std::endl;
}

int main(int argc, char** argv) 
{
	std::string configPath = "webserver.conf";

	if (argc > 2)
	{
		std::cerr << "Usage: ./webserver [config_file]" << std::endl;
		return 1;
	}
	if (argc == 2)
		configPath = argv[1];
	
	signal(SIGPIPE, SIG_IGN);

	try
	{
		ConfigParser config(configPath);
		printConfigParser(config);
		Server s;
		s.init(config);
		s.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
