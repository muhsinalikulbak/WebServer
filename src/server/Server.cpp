#include "Server.hpp"
#include "Socket.hpp"

Server::Server(/* args */)
{
    
}

Server::~Server()
{
    std::map <int, Client*>::iterator it = _clientMap.begin();
    
    while (it != _clientMap.end())
    {
        delete it->second;
        it++;
    }
}

void Server::init(int port)
{
    _masterSocket.createSocket();
    _masterSocket.bindSocket(port);
    _masterSocket.startListening();

    _epollFd = epoll_create1(EPOLL_CLOEXEC); // Bu flag ileride cgi fork attığında kopyalanan epoll fd'yi oto kapatmasını sağlar
    if (_epollFd == -1)
    {
        perror("epoll error");
    }

    std::memset(&_masterEvent, 0, sizeof(_masterEvent));
    
    // Master sockete yeni bir biri bağlantığında bu bir okuma/connect olayıdır. Bu yüzden EPOLLIN ile master sockete epoll_ctl ile ekliyoruz.
    _masterEvent.events = EPOLLIN;
    
    _masterEvent.data.fd = _masterSocket.getFd();

    // (master socket) için içerideki veri akışı, yeni bir istemcinin (client) kapıyı çalıp bağlanmak istemesi demektir.
    // Master socket'i epoll'a ekliyoruz. Artık master socket'e bir bağlantı geldiğinde epoll_wait ile bunu yakalayabileceğiz.
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _masterSocket.getFd(), &_masterEvent) == -1)
    {
        perror("epoll ctrl error");
    }

    _events.resize(100);
}
bool Server::acceptNewConnection()
{
     int clientFd = _masterSocket.acceptConnection();
                    
    if (clientFd == -1)
    {
        perror("Error accept");
        return false;
    }

    struct epoll_event event;
    event.data.fd = clientFd;
    event.events = EPOLLIN; // Bu yeni client'den gelen istekleri dinlemek istiyorum

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1)
    {
        perror("Error epoll add");
        return false;
        // Burada throw fırlatıp program mı sonlanmalı yoksa
        // Sonraki client'lara devam mı etmeli.
    }
    _clientMap[clientFd] = new Client(clientFd);
    return true;
}

void Server::handleClientReceive(int clientFd, epoll_event *event)
{
    Client* client = _clientMap[clientFd];
    StreamState state = client->receiveData();

    if (state == TRANSFER_ERROR || state == PEER_CLOSED)
    {
        // Client bağlantıyı kapattı (EOF) veya hata oluştu
        deleteClient(clientFd, event);
    }
    else if (state == TRANSFER_COMPLETE)
    {
        // 1. Burada Request parse edilip response üretilecek (Şimdilik dummy bir response ekleyelim)
        // Request'i request parser alacak, ardından http_request nesnesi dönecek
        // Bu nesneyi response builder alacak verileri httpresponse nesnesi olarak dönecek.
        // Nesne response'su getResponse() string olarak getirecek.


        std::string dummyResponse = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: keep-alive\r\n\r\nHello World!!\n";
        client->appendToWriteBuffer(dummyResponse);

        event->events = EPOLLOUT;

        if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, event) == -1)
        {
            perror("Error modifying to EPOLLOUT");
            deleteClient(clientFd, event);
        }
        client->clearReadBuffer();
    }
}



void Server::handleClientSend(int clientFd, epoll_event *event)
{
    Client *client = _clientMap[clientFd];
    StreamState state = client->sendData();

    if (state == TRANSFER_ERROR)
    {
        // Gönderim hatası, bağlantıyı kopar
        deleteClient(clientFd, event);
    }
    else if (state == TRANSFER_COMPLETE)
    {
        // Response başarıyla tamamen gönderildi!
        // Keep-Alive aktif olduğu için soketi kapatmıyoruz, yeni istekler için tekrar EPOLLIN moduna alıyoruz.
        event->events = EPOLLIN;
        if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, event) == -1)
        {
            perror("Error modifying back to EPOLLIN");
            deleteClient(clientFd, event);
        }
    }
    // state == TRANSFER_INCOMPLETE ise ellemiyoruz, bir sonraki döngüde kalan veriyi göndermeye devam edecek.
}

void Server::deleteClient(int clientFd, epoll_event* event)
{
    if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, event) == -1)
    {
        perror("error epoll dell");
    }
    close(clientFd);
    delete _clientMap[clientFd];
    _clientMap.erase(clientFd);
}

void Server::run()
{
    int currentFd = 0;

    while (true)
    {
        // Makrolara (EPOLLIN vs) & ile kontrol etmemizin nedeni her bir durumun
        // Tek 1 biti ile temsil edilmesinden kaynaklanır. Ve birden fazla durum var ise
        // kernel bu iki makroru OR'lar ve sonucu öyle verir.
        // Yani örneğin EPOLLIN = 0001 temsil ederken EPOLLHUP 1000'ı temsil edebilir.
        // Ve aynı anda ikisi olduğu zaman bunu OR'lar ve sonuç 1001 olur.
        
        // Bu durumda 32 Bitlik == karşılaştırma yapmak burada hatalıdır.
        // Onun yerine EPOLLIN & 1001 yapıldığında buradan 1 gelir ve 1 True'dur
        // Diğer türlü EPOLLIN == 1001 yapsaydı false olurdu ve isteği kaçırmış olurdu.

        int activeEvents = epoll_wait(_epollFd, &_events[0], _events.size(), -1);

        if (activeEvents == -1)
        {
            perror("epoll wait");
            // throw fırlat
        }

        for (int i = 0; i < activeEvents; i++)
        {
            currentFd = _events[i].data.fd;

            if (_events[i].events & (EPOLLERR | EPOLLHUP))
            {
                // socket üzerinde hata oluştu(kernel tarafından otomatik set edilir)
                // Ya da Bağlantı koptu ya da /hang up (kernel tarafından otomatik set edilir.)
                deleteClient(currentFd, &_events[i]);
            }
            else if (_events[i].events & EPOLLIN)
            {
                if (currentFd == _masterSocket.getFd())
                {
                    // Yeni client'i epoll'a ekliyoruz.
                    acceptNewConnection();
                }
                else
                { 
                    // Var olan client'dan request gelmiş
                    handleClientReceive(currentFd, &_events[i]);
                }
            }
            else if (_events[i].events & EPOLLOUT)
            {
                handleClientSend(currentFd, &_events[i]);
            }
        }
    }
}
