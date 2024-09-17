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
    // 默认析构函数，调用父类 fd 析构完成句柄关闭即可
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
        // 是否发生中断或者错误
        bool hupOrErr(){
            return this->events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
        }
        // 是否存在读就绪事件
        const bool readable()const{
            return this->events & EPOLLIN;
        }
        // 是否存在写就绪事件
        const bool writable()const{
            return this->events & EPOLLOUT;
        }
    };

    // 添加 fd 并注册监听的事件
    void add(Fd::ptr fd, EventType eType, const bool oneshot = true);
    // 针对 fd 修改对其监听的事件
    void modify(Fd::ptr fd, EventType eType, const bool oneshot = true);
    // 移除 fd 
    void remove(int fd);    
    /**
     * @brief：等待并获取到达的事件
     * @param[in]：timeoutMilis：-1——阻塞模式 0——非阻塞模式 >0——指定毫秒作为超时时间
     * @return：返回就绪事件列表
     */
    std::vector<Event::ptr> wait(const int timeoutMilis = -1);

private:
    // epoll 事件表容量
    int m_size;
};


}}