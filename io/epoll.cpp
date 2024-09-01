#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "epoll.h" 
#include "../trace/assert.h" 

namespace cbricks{namespace io{

void setNonblocking(int fd){
    int oldOpt = fcntl(fd,F_GETFL);
    int newOpt = oldOpt | O_NONBLOCK;
    fcntl(fd,F_SETFL,newOpt);
}

// 构造函数
EpollFd::EpollFd(int epollFd):m_epollFd(epollFd){}

// 析构函数
EpollFd::~EpollFd(){
    this->_close();
}

// 注册指定 fd 的读事件
void EpollFd::addEvent(int fd, Event ev){
    CBRICKS_ASSERT(!this->m_closed.load(), "add event in closed epollfd");
    CBRICKS_ASSERT(ev == Read || ev == Write, "add invalid event");
    CBRICKS_ASSERT(fd > 0, "epoll fd add read event with nonpositive fd");
    
    epoll_event e;
    e.data.fd = fd;
    
    if (ev == Read){
        e.events = EPOLLIN | EPOLLET | EPOLLRDHUP; 
    }else{
        e.events = EPOLLOUT | EPOLLET | EPOLLRDHUP; 
    }
    
    // 将 fd 以及关注的事件注册到 epoll fd 中
    int ret = epoll_ctl(this->m_epollFd,EPOLL_CTL_ADD,fd,&e);
    CBRICKS_ASSERT(ret == 0, "epoll ctl failed");

    // 将 fd 设置为非阻塞模式
    setNonblocking(fd);
}

// 移除指定 fd 
void EpollFd::remove(int fd){
    CBRICKS_ASSERT(!this->m_closed.load(), "remove fd in closed epollfd");
    epoll_ctl(this->m_epollFd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}


void EpollFd::_close(){
    this->m_once.onceDo([this](){
        this->m_closed.store(true);
        close(this->m_epollFd);
    });
}


/**
 * epollPool 全局 epoll 存储管理模块
 */

// 构造析构私有
EpollPool::~EpollPool(){
    // 删除所有的 epoll fd
    lock::lockGuard guard(this->m_lock);
    for (auto it : this->m_epollFds){
        it.second->_close();
    }
}

EpollPool& EpollPool::GetInstance(){
    static EpollPool instance;
    return instance;
}

// 创建新的 epoll fd
EpollFd& EpollPool::create(int size){
    int epollFd = epoll_create(size);
    CBRICKS_ASSERT(epollFd > 0, "create epoll failed");

    EpollFd::ptr entity(new EpollFd(epollFd));

    lock::lockGuard guard(this->m_lock);
    this->m_epollFds.insert({epollFd,entity});
    
    return *entity.get();
}

// 删除某个 epoll fd
bool EpollPool::remove(int epollFd){
    lock::lockGuard guard(this->m_lock);
    auto it = this->m_epollFds.find(epollFd);
    if (it == this->m_epollFds.end()){
        return false;
    }
    this->m_epollFds.erase(it);
    return true;
}

std::vector<EpollFd&> EpollPool::getAll(){
    lock::lockGuard guard(this->m_lock);
    std::vector<EpollFd&> res;
    res.reserve(this->m_epollFds.size());
    for (auto it : this->m_epollFds){
        res.push_back(*it.second.get());
    }
    return res;
}


}}