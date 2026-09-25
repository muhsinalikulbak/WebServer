#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "EpollHandler.hpp"
#include <string>
#include <ctime>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>

class Client;

class CgiHandler : public EpollHandler
{
	private:

		int         _stdoutPipeFd;

		int         _stdinPipeFd;

		pid_t       _pid;
		Client*     _owner;
		std::time_t _startTime;

		std::string _cgiOutputBuffer;

		size_t      _stdinWriteOffset;



	public:
		CgiHandler();
		CgiHandler(int stdoutPipeFd, int stdinPipeFd, pid_t pid, Client* owner);
		~CgiHandler();


		int          getStdoutFd() const;
		int          getStdinFd() const;

		int          getFd() const;
		EpollHandler::HandlerType  getType() const;

		pid_t        getPid() const;
		void         setPid(pid_t pid);

		Client*      getOwner() const;
		void         setOwner(Client* owner);

		void         appendOutput(const std::string& data);
		const std::string& getOutputBuffer() const;
		void         closeStdin(int epollFd);

		void               setStdinOffset(size_t offset);
		const char*        stdinRemainingData() const;
		size_t             stdinRemainingSize() const;
		void               consumeStdinBuffer(size_t n);

		std::time_t        getStartTime() const;

};

#endif