#include <stdexcept>
#include <sys/uio.h>

#include "conn.h"
#include "../trace/assert.h"

namespace cbricks{namespace server{

Conn::Conn(int fd):m_fd(fd){}

// 读取全量内容
std::vector<char> Conn::readAll(){
    std::vector<char>body;
    // 一次性从客户端连接中读取全量的内容
    while (true){
        char readBuf [READ_BUF_SIZE];
        int read = recv(this->m_fd,readBuf, READ_BUF_SIZE,0);
        if (read <= 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            throw std::logic_error("read error");
        }
        
        for (int i = 0; i < read; i++){
            body.push_back(readBuf[i]);
        }
    }
    return body;
}

// 写入内容
void Conn::write(std::vector<char>& body){
    iovec iv;
    iv.iov_base = body.data();
    iv.iov_len = body.size();
    writev(this->m_fd,&iv,1);
}

}}