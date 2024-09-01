#pragma once 

#include <memory>

#include "../sync/once.h"
#include "../base/nocopy.h"

namespace cbricks{ namespace io{
class Fd : base::Noncopyable{
public:
    typedef std::shared_ptr<Fd> ptr;
    typedef sync::Once once;

public:
    explicit Fd(int fd);
    virtual ~Fd();
    
public:
    void setNonblocking();
    // 获取句柄值
    const int get()const;
    virtual void closeFd();

protected:
    int m_fd;

private:
    once m_once;
};

}}