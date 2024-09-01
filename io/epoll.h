#pragma once 

#include <memory>
#include <atomic>
#include <unordered_map>
#include <vector>

#include "fd.h"
#include "../sync/lock.h"
#include "../sync/once.h"
#include "../base/nocopy.h"

namespace cbricks{namespace io{

// 一个 epoll 句柄实例
class EpollFd : public Fd{
public:
    typedef std::shared_ptr<EpollFd> ptr;
    typedef sync::Lock lock;

public: 
    // 构造函数与析构函数
    EpollFd(int size);
    ~EpollFd() override;
    
public:
    // 事件类型
    enum EventType{
        Read = 0x001,
        Write = 0x004
    };

    // 到达的事件
    struct Event{
        // 到达的事件
        uint32_t events;
        // 对应的 fd 句柄
        int fd;
        Event(int fd, uint32_t events):fd(fd),events(events){}
    };

    // 注册指定 fd 的事件
    void addEvent(Fd::ptr fd, EventType eType);
    // 移除指定 fd 
    void remove(int fd);    
    // 等待事件到达
    std::vector<Event> wait();

private:    
    // epollFd 是否已关闭
    std::atomic<bool> m_closed{false};
    // 注册在其中的 fd 句柄
    std::unordered_map<int, Fd::ptr> m_fds;

    lock m_lock;
};


}}