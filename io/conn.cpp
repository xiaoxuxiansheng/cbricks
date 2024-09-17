// 声明 recv 函数
#include <netinet/in.h>
// 声明 write 函数
#include <sys/uio.h>
// 声明 close fd 函数
#include <unistd.h>

#include "conn.h"
#include "../trace/assert.h"

namespace cbricks{namespace io{

// 构造函数
ConnFd::ConnFd(int fd):Fd(fd){}

// 从 fd 中读取数据并写入到读缓冲区中. 一次性读取全量
void ConnFd::readFd(){
    // 保证并发安全
    lock::lockGuard guard(this->m_lock);

    char readBuf[READ_BUF_SIZE];
    while (true){
        /**
         * @brief:recv： 从 fd 中读取数据
         *  - @param：1)fd 句柄
         *  - @param：2)存放接收数据的缓冲区
         *  - @param：3)缓冲区长度
         *  - @param：4)0——非阻塞操作
         *  - @return ：读取到的字节数
         */
        // 从 fd 中读取数据
        int read = recv(this->m_fd, readBuf, READ_BUF_SIZE, 0);
        if (!read){
            break;
        }

        if (read < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
                break;
            }
            CBRICKS_ASSERT(false,"recv fail");
        }

        // 读取到内容后，将指定内容写入到读缓冲区中
        this->m_readBuf = this->m_readBuf.append(readBuf,read);
    }
}

// 向 fd 中写入内容
void ConnFd::writeFd(){
    // 保证并发安全
    lock::lockGuard guard(this->m_lock);

    iovec iv;
    iv.iov_base = const_cast<char*>(this->m_writeBuf.c_str());
    iv.iov_len = this->m_writeBuf.size();
    /**
     * @brief:writev：向指定 fd 中写入数据
     *  - @param：1)fd 句柄
     *  - @param：2)iovec 结构体指针. iovec 标识一个缓冲区的地址和长度
     *  - @param: 3)iovec 结构体的数量
     */
    writev(this->m_fd,&iv,1);
}

// 获取读缓冲区中的数据，一次性读取全量
std::string& ConnFd::readFromBuf(){
    lock::lockGuard guard(this->m_lock);
    return this->m_readBuf;
}

// 向写缓冲区中写入数据，为全量覆盖
void ConnFd::writeToBuf(std::string& data){
    // 互斥锁
    lock::lockGuard guard(this->m_lock);
    this->m_writeBuf = data;
}

}}