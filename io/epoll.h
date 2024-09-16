#pragma once 

#include <memory>
#include <vector>
#include <sys/epoll.h>

#include "fd.h"

namespace cbricks{namespace io{

// epoll fd. 继承 fd 
class EpollFd : public Fd{
public:
    // 智能指针别名
    typedef std::shared_ptr<EpollFd> ptr;

public: 
    /** 构造与析构函数 */
    // 构造函数 size——epoll事件表中可添加的最大 fd 数量
    explicit EpollFd(int size);
    ~EpollFd() override = default;
    
public:
    /**
     * epoll 事件类型
     */
    enum EventType{
        // 读事件
        Read = 0x001,
        // 写事件
        Write = 0x004
    };

    /**
     * 就绪的 epoll 事件
     */
    struct Event{
        typedef std::shared_ptr<Event> ptr;
        // 到达的事件
        uint32_t events;
        // 对应的 fd 句柄
        int fd;
        // 构造函数
        Event(int fd, uint32_t events):fd(fd),events(events){}
        bool hupOrErr(){
            return this->events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
        }
        // 读就绪
        const bool readable()const{
            return this->events & EPOLLIN;
        }
        // 写就绪
        const bool writable()const{
            return this->events & EPOLLOUT;
        }
    };

    // 添加 fd 并注册事件
    void add(Fd::ptr fd, EventType eType, const bool oneshot = true);
    // 针对 fd 修改监听事件
    void modify(Fd::ptr fd, EventType eType, const bool oneshot = true);
    // 移除 fd 
    void remove(int fd);    
    // 等待并获取到达的事件. timeout：-1——阻塞模式 0——非阻塞模式 >0——指定毫秒作为超时时间
    std::vector<Event::ptr> wait(const int timeoutMilis = -1);

private:
    int m_size;
};


}}