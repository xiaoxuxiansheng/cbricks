#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>

#include "socket.h"
#include "../trace/assert.h"

namespace cbricks
{
namespace io
{

SocketFd::SocketFd():Fd(socket(PF_INET,SOCK_STREAM,0)){
    linger tmp = {1,1};
    setsockopt(this->m_fd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
}

void SocketFd::bindAndListen(int port){
    CBRICKS_ASSERT(port > 0, "invalid socket port");

    lock::lockGuard guard(this->m_lock);

    CBRICKS_ASSERT(this->m_port.load() == 0, "repeat bind");
    this->m_port.store(port);

    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(this->m_port);

    // socket 和端口绑定
    int flag = 1;
    setsockopt(this->m_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(this->m_fd, (sockaddr *)&address, sizeof(address));
    CBRICKS_ASSERT(ret >= 0, "bind socket fd fail");

    // 开始监听 socket
    ret = listen(this->m_fd, 5);
    CBRICKS_ASSERT(ret >= 0, "listen socket fd fail");
}

}
} // namespace cbricksnamespace io

