#include <sys/socket.h>
#include <unistd.h>

#include "pipe.h"
#include "../trace/assert.h"
#include "../log/log.h"

namespace cbricks{ namespace io{

/** 构造/析构 */    
PipeFd::PipeFd(){
    int pipe[2];
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe);
    CBRICKS_ASSERT(ret != -1, "socketpair failed");
    this->m_pipe[0].reset(new Fd(pipe[0]));
    this->m_pipe[1].reset(new Fd(pipe[1]));
}

// 获取读取端
const Fd::ptr PipeFd::getRecvFd() const{
    return this->m_pipe[0];
}

// 获取写入端
const Fd::ptr PipeFd::getSendFd() const{
    return this->m_pipe[1];
}

// 设置为非阻塞模式
void PipeFd::setNonblocking(){
    this->m_pipe[0]->setNonblocking();
    this->m_pipe[1]->setNonblocking();
}

void PipeFd::sendSig(int sig){
    send(this->m_pipe[1]->get(),(char*)&sig,1,0);
}



}}