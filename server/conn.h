#pragma once 

#include <memory>
#include <netinet/in.h>

#include "../base/nocopy.h"
#include "../sync/lock.h"

namespace cbricks{namespace server{

class Conn : base::Noncopyable{
public:
    typedef std::shared_ptr<Conn> ptr;
    typedef sync::Lock lock;

public:
    // 读数据缓冲区大小 
    static const int READ_BUF_SIZE = 1024;
    // 写数据缓冲区大小
    static const int WRITE_BUF_SIZE  = 1024;

public:
    // 构造/析构
    Conn(int fd);
    virtual ~Conn() = default;

public:
    // 读取全量内容
    void readAll();
    // 写入内容
    void write();

private:
    // 与之对应的 fd 句柄
    int m_fd;

    // 读数据缓冲区
    char m_readBuf[READ_BUF_SIZE];
    long m_readIdx;

    // 写数据缓冲区
    char m_writeBuf[WRITE_BUF_SIZE];
    long m_writeIdx;
};

}}