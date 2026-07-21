#include "Server.hpp"
#include "ConfigParser.hpp"

int main(int argc, char** argv) 
{
	(void)argc;
	(void)argv;
	
	signal(SIGPIPE, SIG_IGN);

	try
	{
		// Mock config üret veya parse et
		Config config = ConfigParser::createMockConfig();

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