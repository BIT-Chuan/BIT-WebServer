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
#include"../http/http.h"

class WebServer{
public:
    WebServer(int port, int threadNum, int close_log, std::string user, std::string passwd, std::string sqlname);
    ~WebServer();
    void run();
private:
    void setNonBlock(int fd);
    bool initListenSocket();
    void dealListenEvent();
    void addClient(int fd, struct sockaddr_in addr);
    void closeHttpConn(int fd);
    void dealReadEvent(Http* client);
    void dealWriteEvent(Http* client);
    void dealRead(Http* client);
    void dealWrite(Http* client);
private:
    static const int MAX_FD = 65535;
    int m_port;
    int listenFd;
    char* sourceDir;
    uint32_t listen_events;
    uint32_t conn_events;
    bool isClosed;
    int m_closeLog;
    std::string m_user;
    std::string m_passwd;
    std::string m_sqlname;

    std::unique_ptr<ThreadPool>m_threadpool;
    std::unique_ptr<Epoller>m_epoller;
    std::unordered_map<int, Http>m_users;
};

#endif