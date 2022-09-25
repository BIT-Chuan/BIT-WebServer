#include"webserver.h"

WebServer::WebServer(int port, int threadNum)
    : m_port(port), m_threadpool(new ThreadPool(threadNum)), m_epoller(new Epoller())
{
    sourceDir = getcwd(nullptr, 256);
    strncat(sourceDir, "/root/", 6);
    
    isClosed = true;
    listen_events = EPOLLRDHUP | EPOLLET;
    conn_events = EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
    
    if(!initListenSocket()){
        isClosed = true;
    }
}

WebServer::~WebServer(){
    close(listenFd);
    isClosed = true;
    free(sourceDir);
}

bool WebServer::initListenSocket(){
    listenFd = socket(PF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = m_port;
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(listenFd, (struct sockaddr*)&address, sizeof(address));
    if(ret < 0){
        return false;
    }

    ret = listen(listenFd, 0);
    if(ret < 0){
        return false;
    }

    m_epoller->addFd(listenFd, listen_events | EPOLLIN);
    setNonBlock(listenFd);

    return true;
}

void WebServer::setNonBlock(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void WebServer::run(){
    while(!isClosed){
        int count = m_epoller->epollWait();
        for(int i = 0; i < count; ++i){
            int fd = m_epoller->getEventFd(i);
            uint32_t events = m_epoller->getEvents(i);
            if(fd == listenFd){
                dealListenEvent();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                closeHttpConn();
            }
            else if(events & EPOLLIN){
                dealReadEvent();
            }
            else if(events & EPOLLOUT){
                dealWriteEvent();
            }
        }
    }
    return;
}

void WebServer::dealListenEvent(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    while(true){
        int fd = accept(listenFd, (struct sockaddr*)&addr, &len);
        if(fd <= 0){
            break;
        }
        addClient(fd, addr);
    }
    return;
}

void WebServer::addClient(int fd, struct sockaddr_in addr){
    //初始化一个http连接对象
    m_epoller->addFd(fd, EPOLLIN | conn_events);
    setNonBlock(fd);
}

void WebServer::closeHttpConn(){

}

void WebServer::dealReadEvent(){

}

void WebServer::dealWriteEvent(){

}