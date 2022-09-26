#include"webserver.h"

WebServer::WebServer(int port, int threadNum, int close_log, std::string user, std::string passwd, std::string sqlname)
    : m_port(port), m_threadpool(new ThreadPool(threadNum)), m_epoller(new Epoller()),
    m_closeLog(close_log), m_user(user), m_passwd(passwd), m_sqlname(sqlname)
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
                closeHttpConn(fd);
            }
            else if(events & EPOLLIN){
                dealReadEvent(&m_users[fd]);
            }
            else if(events & EPOLLOUT){
                dealWriteEvent(&m_users[fd]);
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
    m_users[fd].init(fd, addr, sourceDir, m_closeLog, m_user, m_passwd, m_sqlname);
    m_epoller->addFd(fd, EPOLLIN | conn_events);
    setNonBlock(fd);
}

void WebServer::closeHttpConn(int fd){
    m_users.erase(fd);
    m_epoller->delFd(fd);
    close(fd);
}

void WebServer::dealReadEvent(Http* client){
    m_threadpool->addTask(std::bind(&WebServer::dealRead, this, client));
}

void WebServer::dealWriteEvent(Http* client){
    m_threadpool->addTask(std::bind(&WebServer::dealWrite, this, client));
}

void WebServer::dealRead(Http* client){
    client->read_once();
    if(client->process()){
        m_epoller->modFd(client->getFd(), EPOLLIN | conn_events);
    }
    else{
        m_epoller->modFd(client->getFd(), EPOLLOUT | conn_events);
    }
}

void WebServer::dealWrite(Http* client){
    int save_errno = 0;
    client->write(&save_errno);
    if(save_errno == EAGAIN){
        m_epoller->modFd(client->getFd(), EPOLLOUT | conn_events);
    }
    else{
        m_epoller->modFd(client->getFd(), EPOLLIN | conn_events);
    }
}