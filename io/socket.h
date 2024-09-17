#pragma once 

#include <memory>

#include "fd.h"
#include "../sync/lock.h"

namespace cbricks{namespace io{

/**
 * socketFd 继承 fd
 */
class SocketFd : public Fd{
public:
    // 智能指针 类型别名
    typedef std::shared_ptr<SocketFd> ptr;
    // 互斥锁 类型别名
    typedef sync::Lock lock;
public:
    /** 构造/析构函数 */
    SocketFd();
    // 默认析构函数，复用父类 fd 析构完成句柄关闭即可
    ~SocketFd() override = default;

public:
    // 绑定指定端口并开始监听
    void bindAndListen(int port);

private:
    // 监听的端口
    int m_port;
    // 互斥锁
    lock m_lock;
};


}} // namespace cbricksnamespace io

