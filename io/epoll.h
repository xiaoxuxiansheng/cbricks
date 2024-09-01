#pragma once 

#include <memory>
#include <atomic>
#include <unordered_map>
#include <vector>

#include "../sync/lock.h"
#include "../sync/once.h"
#include "../base/nocopy.h"

namespace cbricks{namespace io{

// 将某个 fd 设置为非阻塞模式
void setNonblocking(int fd);

// epoll 池
class EpollPool;

// 一个 epoll 句柄实例
class EpollFd{
friend class EpollPool;
public:
    typedef std::shared_ptr<EpollFd> ptr;
    typedef sync::Once once;

private: 
    // 构造函数
    EpollFd(int epollFd);
    // 析构函数
    ~EpollFd();
    
public:
    enum Event{
        Read,
        Write
    };

    // 注册指定 fd 的事件
    void addEvent(int fd, Event ev);
    // 移除指定 fd 
    void remove(int fd);

private:
    void _close();

private:    
    once m_once;
    std::atomic<bool> m_closed{false};

    // 真正的 epoll 句柄
    int m_epollFd;
};


class EpollPool : base::Noncopyable{
public:
    typedef sync::Lock lock;

// 全局单例
public:
    static EpollPool& GetInstance();

public:
    // 创建新的 epoll fd
    EpollFd& create(int size);
    // 删除某个 epoll fd
    bool remove(int epollFd);
    std::vector<EpollFd&> getAll();
    
private:
    // 构造析构私有
    EpollPool() = default;
    ~EpollPool();

private:
    lock m_lock;
    // 管理的 epoll fd
    std::unordered_map<int,EpollFd::ptr> m_epollFds;
};



}}