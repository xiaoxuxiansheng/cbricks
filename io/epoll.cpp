#include <sys/epoll.h>

#include "epoll.h" 
#include "../trace/assert.h" 

namespace cbricks{namespace io{

const int MAX_EPOLL_EVENT_NUM = 10000; //最大事件数

// 构造函数
EpollFd::EpollFd(int size) : Fd(epoll_create(size)){}

// 析构函数
EpollFd::~EpollFd(){
    this->m_closed.store(true);
    Fd::~Fd();
}

// 注册指定 fd 的读事件
void EpollFd::addEvent(Fd::ptr fd, EventType eType){
    CBRICKS_ASSERT(!this->m_closed.load(), "add event in closed epollfd");
    CBRICKS_ASSERT(eType == Read || eType == Write, "add invalid event");

    int targetFd = fd->get();
    CBRICKS_ASSERT(targetFd > 0, "epoll fd add read event with nonpositive fd");
    
    epoll_event e;
    e.data.fd = targetFd;
    
    if (eType == Read){
        e.events = EPOLLIN | EPOLLET | EPOLLRDHUP; 
    }else{
        e.events = EPOLLOUT | EPOLLET | EPOLLRDHUP; 
    }

    // 将 fd 设置为非阻塞模式
    fd->setNonblocking(); 
    
    // 将 fd 以及关注的事件注册到 epoll fd 中
    int ret = epoll_ctl(this->m_fd,EPOLL_CTL_ADD,targetFd,&e);
    CBRICKS_ASSERT(ret == 0, "epoll ctl failed"); 

    // fd 添加到池子中
    lock::lockGuard guard(this->m_lock);
    // 不存在即插入，存在即忽略
    this->m_fds.insert({targetFd,fd});
}

// 移除指定 fd 
void EpollFd::remove(int fd){
    CBRICKS_ASSERT(!this->m_closed.load(), "remove fd in closed epollfd");
    epoll_ctl(this->m_fd,EPOLL_CTL_DEL,fd,0);
    // 从池子中移除
    lock::lockGuard guard(this->m_lock);
    this->m_fds.erase(fd);   
}

// 等待事件到达
std::vector<EpollFd::Event> EpollFd::wait(){
    epoll_event rawEvents[MAX_EPOLL_EVENT_NUM];
    int ret = epoll_wait(this->m_fd, rawEvents, MAX_EPOLL_EVENT_NUM, -1);
    
    std::vector<EpollFd::Event> events;
    if (errno == EINTR){
        return events;
    }

    CBRICKS_ASSERT(ret >= 0, "epoll wait fail");
    
    for (int i = 0; i < ret; i++){
        events.push_back(Event(rawEvents[i].data.fd,rawEvents[i].events));
    }

    return events;
}

}}