#ifndef EPOLLER_H
#define EPOLLER_H

#include<vector>
#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>

class Epoller{
public:
    Epoller(int maxEvents = 1024);
    ~Epoller();
    void addFd(int fd, uint32_t events);
    void delFd(int fd);
    void modFd(int fd, uint32_t events);
    int getEventFd(int i) const;
    uint32_t getEvents(int i) const;
    int epollWait();
private:
    int epollFd;
    std::vector<struct epoll_event>epollEvents;
};

#endif