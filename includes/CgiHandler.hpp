#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "EpollHandler.hpp"
#include <string>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>

class Client;

// CgiHandler, bir CGI script'inin child process'i ile aramızdaki
// pipe'ları ve process bilgisini tutan nesnedir.
// Her CGI çalıştırıldığında iki ayrı pipe() açılır:
//
//  1) STDOUT pipe'ı -> CGI script'in ürettiği response'u okumak için
//     pipe(stdoutPipe) sonrası:
//       stdoutPipe[0] = okuma ucu  -> PARENT (biz) tutar, epoll'a bunu ekleriz (EPOLLIN)
//       stdoutPipe[1] = yazma ucu  -> CHILD kendi stdout'una dup2 eder, parent hemen kapatır
//
//  2) STDIN pipe'ı  -> client'tan gelen body'yi CGI script'e yazmak için
//     pipe(stdinPipe) sonrası:
//       stdinPipe[1] = yazma ucu  -> PARENT (biz) tutar, epoll'a bunu ekleriz (EPOLLOUT)
//       stdinPipe[0] = okuma ucu  -> CHILD kendi stdin'ine dup2 eder, parent hemen kapatır
//
// Yani biz her zaman "kendi kullanacağımız" ucu tutuyoruz, child'a verdiğimiz
// ucu fork sonrası hemen kapatıyoruz (aksi halde pipe hiç EOF vermez).

class CgiHandler : public EpollHandler
{
	private:

		// Okuma ucundan okuduğumuz bizim cgi çıktımız olduğu için
		// Değişken ismi stdout olarak ayarlandı.

		int         _stdoutPipeFd;   // pipe'ın okuma ucu (fd[0]) - CGI'nin ürettiği veriyi buradan okuruz

		// Body script'e vereceğimiz input argümanı olduğu için
		// değişken ismi stdin olarak ayarlandı.
		int         _stdinPipeFd;    // pipe'ın yazma ucu (fd[1]) - client body'sini buraya yazarız

		pid_t       _pid;
		Client*     _owner;          // Bu CGI hangi client için çalışıyor, response'u ona yazacağız

		std::string _cgiOutputBuffer; // stdout pipe'ından biriktirdiğimiz ham CGI çıktısı
		std::string _stdinWriteBuffer; // client'tan gelen ama henüz script'e yazılmamış body kısmı
		// Bu direk set edilir, sonra write() ile parça parça gideceği için buffer boşalanana kadar yazılır



	public:
		CgiHandler();
		CgiHandler(int stdoutPipeFd, int stdinPipeFd, pid_t pid, Client* owner);
		~CgiHandler();


		int          getStdoutFd() const;
		int          getStdinFd() const;

		/**** EpollHandler OVERRIDE ****/

		// Primary fd olarak stdout pipe'ını döneriz (response okuma tarafı asıl akışı yönetir)
		int          getFd() const;
		HandlerType  getType() const;

		/**** PID ****/

		pid_t        getPid() const;
		void         setPid(pid_t pid);

		/**** OWNER ****/

		Client*      getOwner() const;
		void         setOwner(Client* owner);

};

#endif