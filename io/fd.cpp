// 声明了 close fd 函数
#include <unistd.h>
// 声明了 fcntl 函数，用于设定 fd 模式
#include <fcntl.h>

#include "fd.h"
#include "../trace/assert.h"

namespace cbricks{ namespace io{

// 构造函数
Fd::Fd(int fd):m_fd(fd){
    CBRICKS_ASSERT(this->m_fd > 0, "nonpositive fd");
}

// 析构函数
Fd::~Fd(){close(this->m_fd);}

// 获取 fd 句柄值
const int Fd::get()const{
    return this->m_fd;
}

// 将 fd 句柄设置为非阻塞模式
void Fd::setNonblocking(){
    int oldOpt = fcntl(this->m_fd,F_GETFL);
    int newOpt = oldOpt | O_NONBLOCK;
    fcntl(this->m_fd,F_SETFL,newOpt);
}
}}