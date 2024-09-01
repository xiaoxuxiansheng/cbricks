#pragma once 

#include <memory>
#include <atomic>

#include "fd.h"
#include "../sync/lock.h"

namespace cbricks
{
namespace io
{

class SocketFd : public Fd{
public:
    typedef std::shared_ptr<SocketFd> ptr;
    typedef sync::Lock lock;
public:
    // 创建 socket fd
    SocketFd();

    // 绑定指定端口并开始监听
    void bindAndListen(int port);

private:
    std::atomic<int>m_port{0};
    lock m_lock;
};


} 
} // namespace cbricksnamespace io

