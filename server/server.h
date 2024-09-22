#pragma once 

#include <functional>
#include <atomic>
#include <unordered_map>

#include "../base/nocopy.h"
#include "../base/defer.h"
#include "../base/sys.h"
#include "../io/socket.h"
#include "../io/epoll.h"
#include "../io/conn.h"
#include "../io/pipe.h"
#include "../sync/lock.h"
#include "../sync/sem.h"
#include "../sync/once.h"
#include "../pool/workerpool.h"

namespace cbricks{namespace server{

/**
 * 底层基于 epoll 技术实现的服务器
 */
class Server : base::Noncopyable{
public:
    // 智能指针类型别名
    typedef std::shared_ptr<Server> ptr;

    /**
     * 并发调度相关
     */
    // 互斥锁类型别名
    typedef sync::Lock lock;
    // 信号量类型别名
    typedef sync::Semaphore semaphore;
    // 单例工具 类型别名
    typedef sync::Once once;
    // 协程调度框架 类型别名
    typedef pool::WorkerPool workerPool;

    /**
     * io 相关
     */
    // fd 类型别名
    typedef io::Fd fd;
    // epollFd 类型别名
    typedef io::EpollFd epoll;
    // epoll 事件 类型别名 
    typedef io::EpollFd::Event event;
    // socketFd 类型别名
    typedef io::SocketFd socket;
    // connFd 类型别名
    typedef io::ConnFd conn;
    // pipeFd 类型别名
    typedef io::PipeFd pipe;

    /**
     * @brief:callback：回调函数 类型别名
     *   - @param:——请求数据
     *   - @return:——给予的响应数据
     */
    typedef std::function< std::string( std::string& )> callback;
    // 函数栈回收前执行的 defer 函数 类型别名
    typedef base::Defer defer;

public:
    /** 构造/析构函数 */
    Server() = default;
    virtual ~Server();

public:
    // 默认最大同时处理的事件数，用于初始化 epoll 池作为其容量
    static const int MAX_REQUEST = 8192;
    /**
     * @brief: server
     *  - @param:port：启动的端口号
     *  - @param:cb：当有请求到达后的处理函数 
     *  - @param:threads：启动的线程数
     *  - @param:maxRequest：最大同时处理的事件数
     */
    void init(int port, callback cb, const int threads = 8, int maxRequest = MAX_REQUEST);

    // 运行 server. 要求必须先完成过初始化
    virtual void serve();

private:
    // 运行前处理
    void prepare();
    // 运行中
    void serving();
    // 处理到达的就绪事件
    bool process(event::ptr e);
    // 处理到达的信号量
    bool processSignal();
    // 处理到达的客户端连接
    void spawnConn();
    // 执行就绪的读操作
    void processRead(int _fd);
    // 执行就绪的写操作
    void processWrite(int _fd);
    // [并发安全] 获取指定连接
    conn::ptr getConn(int _fd);
    // [并发安全] 释放指定连接
    void freeConn(int _fd);

private:
    // 静态方法：初始化管道 pipe
    static void pipePrepare();

private:
    // 与 epoll server 一一对应的 socket fd. 在 init 方法中根据传入的 port 完成初始化
    fd::ptr m_socket;
    // 与 epoll server 一一对应的 epoll fd
    epoll::ptr m_epoll;
    // 互斥锁，保证并发安全
    lock m_lock;
    // 记录 server 是否已完成初始化
    std::atomic<bool> m_initialized{false};
    // 记录 server 是否处在运行中
    std::atomic<bool> m_serving{false};

    // 与 epoll server 一一对应的端口
    int m_port;
    // 最大同时处理的请求数量
    int m_maxRequest;
    // 指定事件到达后的回调函数
    callback m_cb;
    // 协程调度框架
    workerPool::ptr m_workerPool;
    // 缓存活跃的连接
    std::unordered_map<int,conn::ptr> m_conns;

private:
    // 静态变量：单例工具，保证 s_pipe 仅被初始化一次
    static once s_once;
    // 静态变量：基于 pipe 实现优雅关闭. pipe 为静态变量，全局共享.
    static pipe::ptr s_pipe;
};


}}