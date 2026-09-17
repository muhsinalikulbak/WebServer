#include "Server.hpp"
#include "ConfigParser.hpp"
#include <exception>
#include <iostream>
#include <csignal>

// volatile: Derleyici optimizasyonlarını engeller, her erişimde gerçek RAM'den okuma yapar
// sig_atomic_t: Signal-safe atomik integer tipi, signal handler içinde güvenli kullanım için
// g_shutdownRequested: Global shutdown flag, signal handler tarafından set edilir
volatile sig_atomic_t g_shutdownRequested = 0;

// Signal handler: SIGINT (Ctrl+C) ve SIGTERM sinyalleri için
// Signal handler'lar programın normal akışını keserek çalışır (async context)
// Bu yüzden sadece async-signal-safe fonksiyonlar çağrılmalıdır (malloc, printf vb. YASAK)
// sig_atomic_t tipi signal-safe olduğu için sadece bu değişkene atomik yazma yapılır
// Main loop bu flag'i kontrol ederek graceful shutdown yapar (bağlantıları düzgün kapatır)
static void handleShutdownSignal(int signum)
{
	(void)signum;
	g_shutdownRequested = 1;
}

int main(int argc, char** argv) 
{
	
	if (argc != 2)
	{
		std::cerr << "Usage: ./webserver [config_file]" << std::endl;
		return 1;
	}
	std::string configPath = argv[1];
	
	// SIGPIPE: Server client'a veri gönderirken client aniden kapanarsa kernel bu sinyali gönderir
	// Varsayılan davranış process'i kill eder, bu yüzden yoksayıyoruz
	// Böylece send() -1 döndürür ve biz bunu hata olarak ele alırız
	signal(SIGPIPE, SIG_IGN);
	
	// SIGINT: Terminal'de Ctrl+C ile gönderilir, graceful shutdown için handler'a yönlendirilir
	// Kullanıcı server'ı durdurmak istediğinde bağlantılar düzgün kapanır
	signal(SIGINT, handleShutdownSignal);
	
	// SIGTERM: kill <pid> komutu ile gönderilir, graceful shutdown için handler'a yönlendirilir
	// Systemd veya process manager'lar server'ı durdurmak için bu sinyali kullanır
	signal(SIGTERM, handleShutdownSignal);

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
