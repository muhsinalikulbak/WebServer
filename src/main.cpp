#include "Server.hpp"

int main() 
{

  signal(SIGPIPE, SIG_IGN);

  try
   {
    Server s;
    s.init(8080);
    s.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
