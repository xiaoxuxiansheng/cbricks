#include <sys/socket.h>
#include <unistd.h>

#include "pipe.h"
#include "../trace/assert.h"
#include "../log/log.h"

namespace cbricks{ namespace io{

/** 构造函数 */    
PipeFd::PipeFd(){
    int pipe[2];
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe);
    CBRICKS_ASSERT(ret != -1, "socketpair failed");
    this->m_pipe[0].reset(new Fd(pipe[0]));
    this->m_pipe[1].reset(new Fd(pipe[1]));
}

// 获取读取端 fd
const Fd::ptr PipeFd::getRecvFd() const{
    return this->m_pipe[0];
}

// 获取写入端 fd
const Fd::ptr PipeFd::getSendFd() const{
    return this->m_pipe[1];
}

// 将 pipe 设置为非阻塞模式
void PipeFd::setNonblocking(){
    this->m_pipe[0]->setNonblocking();
    this->m_pipe[1]->setNonblocking();
}

// 向写入端发送信号
void PipeFd::sendSig(int sig){
    send(this->m_pipe[1]->get(),(char*)&sig,1,0);
}

}}