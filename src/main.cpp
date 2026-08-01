#include "Server.hpp"
#include "ConfigParser.hpp"
#include <exception>
#include <iostream>
#include <csignal>


int main(int argc, char** argv) 
{
	
	if (argc != 2)
	{
		std::cerr << "Usage: ./webserver [config_file]" << std::endl;
		return 1;
	}
	std::string configPath = argv[1];
	
	signal(SIGPIPE, SIG_IGN);

	try
	{
		ConfigParser config(configPath);
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
