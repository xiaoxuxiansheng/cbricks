#pragma once 

#include <memory>

#include "../base/nocopy.h"

namespace cbricks{ namespace io{
/**
 * file descriptor 文件句柄
 */
class Fd : base::Noncopyable{
public:
    // 智能指针别名
    typedef std::shared_ptr<Fd> ptr;

public:
    /**
     * 构造/析构函数
     */
    explicit Fd(int fd);
    virtual ~Fd();
    
public:
    // 设置为非阻塞模式
    void setNonblocking();
    // 获取 fd 句柄值
    const int get() const;

protected:
    // fd 句柄
    int m_fd;
};

}}