#include"epoller.h"

Epoller::Epoller(int maxEvents) : epollFd(epoll_create(512)), epollEvents(maxEvents) {}

Epoller::~Epoller(){
    close(epollFd);
}

int Epoller::addFd(int fd, uint32_t events){
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev);
}

void Epoller::delFd(int fd){
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void Epoller::modFd(int fd, uint32_t events){
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev);
}

int Epoller::getEventFd(int i) const {
    return epollEvents[i].data.fd;
}

uint32_t Epoller::getEvents(int i) const {
    return epollEvents[i].events;
}

int Epoller::epollWait(){
    return epoll_wait(epollFd, &epollEvents[0], epollEvents.size(), -1);
}