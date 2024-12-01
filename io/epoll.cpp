#include "epoll.h" 
#include "../trace/assert.h" 

namespace cbricks{namespace io{

// 构造函数. 调用 epollCreate 完成 epollFd 创建
EpollFd::EpollFd(int size) : Fd(epoll_create(size)),m_size(size){
    CBRICKS_ASSERT(this->m_size > 0, "epoll create with nonpositive size");
}

// 注册针对 fd 的指定事件
void EpollFd::add(Fd::ptr fd, EventType eType, const bool oneshot){
    int targetFd = fd->get();
    CBRICKS_ASSERT(targetFd > 0, "epoll fd add event with nonpositive fd");

    // 将 fd 设置为非阻塞模式
    fd->setNonblocking(); 

    epoll_event e;
    e.data.fd = targetFd;
    
    /**
     * 监听的事件类型. 统一采用 oneshot 和 et 模式
     * et——edge trigger mode：当有就绪事件到达时，必须一次性将数据处理干净
     * oneshot：fd 就绪事件到达后只能被一个线程所处理，直到再次重置 EPOLLONESHOT 标识
     */
    if (eType == Read){
        e.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }else{
        e.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
    }

    if (oneshot){
        e.events = e.events | EPOLLONESHOT;
    }
    
    // 将 fd 以及关注的事件注册到 epoll fd 中
    int ret = epoll_ctl(this->m_fd,EPOLL_CTL_ADD,targetFd,&e);
    CBRICKS_ASSERT(ret == 0, "epoll ctl failed"); 
}

// 针对 fd 修改监听事件
void EpollFd::modify(Fd::ptr fd, EventType eType, const bool oneshot){
    int targetFd = fd->get();
    CBRICKS_ASSERT(targetFd > 0, "epoll fd modify event with nonpositive fd");

    epoll_event e;
    e.data.fd = targetFd;
    
    /**
     * 监听的事件类型. 统一采用 oneshot 和 et 模式
     * et——edge trigger mode：当有就绪事件到达时，必须一次性将数据处理干净
     * oneshot——：fd 就绪事件到达后只能被一个线程所处理，直到再次重置 EPOLLONESHOT 标识
     */
    if (eType == Read){
        e.events = EPOLLIN | EPOLLET | EPOLLRDHUP; 
    }else{
        e.events = EPOLLOUT | EPOLLET | EPOLLRDHUP; 
    }

    if (oneshot){
        e.events = e.events | EPOLLONESHOT;
    }

    // 将 fd 以及关注的事件注册到 epoll fd 中
    int ret = epoll_ctl(this->m_fd,EPOLL_CTL_MOD,targetFd,&e);
    CBRICKS_ASSERT(ret == 0, "epoll ctl failed"); 
}

// 从 epoll 事件表中移除指定 fd
void EpollFd::remove(int fd){
    epoll_ctl(this->m_fd,EPOLL_CTL_DEL,fd,0);
}

// 等待监听事件的到达
std::vector<EpollFd::Event::ptr> EpollFd::wait(const int timeoutMilis){
    std::vector<EpollFd::Event::ptr> events;
    epoll_event rawEvents[this->m_size];
    int ret = epoll_wait(this->m_fd, rawEvents, this->m_size, timeoutMilis);
    
    // 操作被中断，需要重新处理
    if (ret < 0 && errno == EINTR){
        return events;
    }

    // 没有获取到就绪事件
    if (!ret){
        return events;
    }

    CBRICKS_ASSERT(ret > 0, "epoll wait fail");
    
    events.reserve(ret);
    for (int i = 0; i < ret; i++){
        events.emplace_back(new Event(rawEvents[i].data.fd,rawEvents[i].events));
    }

    return events;
}

}}