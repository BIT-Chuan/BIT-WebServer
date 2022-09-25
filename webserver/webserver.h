#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<cstring>
#include<unordered_map>

#include"../pool/threadpool.h"
#include"../epoll/epoller.h"

class WebServer{
public:
    WebServer(int port, int threadNum);
    ~WebServer();
    void run();
private:
    void setNonBlock(int fd);
    bool initListenSocket();
    void dealListenEvent();
    void addClient(int fd, struct sockaddr_in addr);
    void closeHttpConn();
    void dealReadEvent();
    void dealWriteEvent();
private:
    static const int MAX_FD = 65535;
    int m_port;
    int listenFd;
    char* sourceDir;
    uint32_t listen_events;
    uint32_t conn_events;
    bool isClosed;

    std::unique_ptr<ThreadPool>m_threadpool;
    std::unique_ptr<Epoller>m_epoller;
};

#endif