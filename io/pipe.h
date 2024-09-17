#pragma once 

#include <memory>

#include "fd.h"

namespace cbricks{ namespace io{

// 将管道 pipe 具象化为 fd
class PipeFd{

public:
    // 智能指针别名
    typedef std::shared_ptr<PipeFd> ptr;

public:
    /** 构造/析构 */
    PipeFd();
    // 无需显式设置析构函数，因为 pipe 使用封装过的 fd 类型的智能指针，失去引用后会自动析构
    ~PipeFd() = default;

public:
    // 获取接收端 fd
    const Fd::ptr getRecvFd() const;
    // 获取发送端 fd
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