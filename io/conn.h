#pragma once 

#include <memory>
#include <string>

#include "../sync/lock.h"
#include "fd.h"

namespace cbricks{namespace io{
/**
 * @brief：将一笔连接具象化为一个 fd 句柄. 继承 fd，具有不可值复制和值拷贝的性质
 */
class ConnFd : public Fd{
public:
    // 智能指针 类型别名
    typedef std::shared_ptr<ConnFd> ptr;
    // 互斥锁 类型别名
    typedef sync::Lock lock;

public:
    // 单次读操作数据量上限为 16K 
    static const int READ_BUF_SIZE = 16 * 1024;
    // 单次写操作数据量上限为 16K
    static const int WRITE_BUF_SIZE  = 16 * 1024;

public:
    /**
     * 构造/析构函数
     */
    // 构造函数，需要显式传入 fd 句柄
    explicit ConnFd(int fd);
    // 析构函数，声明为 virtual 表示作为可继承类
    // 设置为默认，复用父类 fd 析构逻辑完成句柄关闭即可
    virtual ~ConnFd() override = default;

public:
    // 读取 fd 中的所有数据，存入读缓冲区中，因此获得数据需要调用 readFromBuf 方法
    void readFd();
    // 向 fd 中写入数据，数据来自写缓冲区，因此需要先调用 writeToBuf 方法
    void writeFd();

    // 获取读缓冲区中的数据，一次性读取全量
    std::string& readFromBuf();
    // 向写缓冲区中写入数据，为全量覆盖
    void writeToBuf(std::string& body);

private:
    // 互斥锁 保证并发安全
    lock m_lock;
    // 读缓冲区
    std::string m_readBuf;
    // 写缓冲区
    std::string m_writeBuf;
};

}}