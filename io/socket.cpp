// socket 相关方法
#include <sys/socket.h>
#include <netinet/in.h>
// 提供 bzero 方法
#include <strings.h>

#include "socket.h"
#include "../trace/assert.h"

namespace cbricks{namespace io{

/**
 * 构造函数：
 * - socket：创建 socket 套接字实例
 *   - param1：地址族，指定地址类型；param2：指定套接字类型；param3：指定协议类型
 */
SocketFd::SocketFd():Fd(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)),m_port(0){
    linger tmp = {1,1};
    /**
     * setsockopt 设置套接字配置项：
     *   - param1：socket fd 句柄
     *   - param2：协议层. SOL_SOCKET 为基本套接字
     *   - param3：选项名
     *   - param4：指向选项值的指针
     *   - param5：选项值的长度
     */
    setsockopt(this->m_fd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
}

// 绑定并监听指定端口
void SocketFd::bindAndListen(int port){
    CBRICKS_ASSERT(port > 0, "invalid socket port");

    // 加锁保证只能一个 socketFd 只能监听一个端口
    {
        lock::lockGuard guard(this->m_lock);
        CBRICKS_ASSERT(this->m_port == 0, "repeat bind");
        this->m_port = port;
    }


    sockaddr_in address;
    /**
     * bzero： 将一段内存区域的值都设置为 0
     */
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    /**
     * htonl：实现将主机字节顺序转换为网络字节顺序，确保在网络中传输的正确性
     */
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    /**
     * htons：将无符号短整型主机数值转换为网络字节顺序
     */
    address.sin_port = htons(this->m_port);

    // socket 和端口绑定
    int flag = 1;
    setsockopt(this->m_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    /**
     * bind：将 fd 和 sockaddr 绑定在一起
     */
    int ret = bind(this->m_fd, (sockaddr *)&address, sizeof(address));
    CBRICKS_ASSERT(ret >= 0, "bind socket fd fail");

    /**
     * listen：监听 socket fd，后续能够通过 accpet 接收来自客户端的连接
     *   - param1：socket fd
     *   - param2：挂起连接队列的最大长度
     */
    ret = listen(this->m_fd, SOMAXCONN);
    CBRICKS_ASSERT(ret >= 0, "listen socket fd fail");
}

}} // namespace cbricksnamespace io

