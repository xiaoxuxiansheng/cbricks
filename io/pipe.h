#pragma once 

#include <memory>

#include "fd.h"

namespace cbricks{ namespace io{

// 管道 pipe fd
class PipeFd{

public:
    // 智能指针别名
    typedef std::shared_ptr<PipeFd> ptr;

public:
    /** 构造/析构 */
    PipeFd();
    ~PipeFd() = default;

public:
    // 获取接收端
    const Fd::ptr getRecvFd() const;
    // 获取发送端
    const Fd::ptr getSendFd() const;
    // 向发送端发送信号
    void sendSig(int sig);
    // 设置为非阻塞模式
    void setNonblocking();


private:
    // index——0 接收端
    // index——1 发送端
    Fd::ptr m_pipe[2];
};

}}