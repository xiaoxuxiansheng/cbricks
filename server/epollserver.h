#pragma once 

#include <functional>
#include <atomic>

#include "../base/nocopy.h"
#include "../io/socket.h"
#include "../io/epoll.h"
#include "../sync/lock.h"


namespace cbricks{namespace server{

class EpollServer : base::Noncopyable{
public:
    typedef sync::Lock lock;
    typedef io::EpollFd epollFd;
    typedef io::SocketFd socketFd;
    typedef io::Fd fd;
    typedef std::function<void( io::EpollFd::Event& e )> callback;

public:
    // 构造析构
    EpollServer() = default;
    ~EpollServer() = default;

public:
    // 初始化
    void init(int port, callback cb);

    // 启动
    void listenAndServe();

    // 停止
    void close();

private:
    void servePreprocess();
    void serve();
    void process(epollFd::Event& event);
    void processConn();
    void processRead();
    void processWrite();

private:
    // socket fd
    socketFd::ptr m_socketFd;
    // epoll fd
    epollFd::ptr m_epollFd;

    lock m_lock;
    std::atomic<bool> m_served{false};
    std::atomic<bool> m_closed{false};

    // 端口 回调函数
    int m_port;
    callback m_cb;
};


}}