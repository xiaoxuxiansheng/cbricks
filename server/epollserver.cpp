#include <netinet/in.h>
#include <sys/socket.h>

#include "epollserver.h"
#include "../trace/assert.h"
#include "../log/log.h"

namespace cbricks{namespace server{

// 初始化
void EpollServer::init(int port, callback cb){
    this->m_cb = cb;
    this->m_port = port;
}

// 启动
void EpollServer::listenAndServe(){
    CBRICKS_ASSERT(this->m_cb != nullptr, "empty callback when running server");
    CBRICKS_ASSERT(this->m_port > 0, "invalid port when running server");

    {
        lock::lockGuard guard(this->m_lock);
        CBRICKS_ASSERT(!this->m_served.load(), "repeat server");
        this->m_served.store(true);
    } 

    /**
     * 前处理：
     * - 创建 socketFd
     * - epollCreate
     * - socketFd 添加到 epollFd 中
     */
    this->servePreprocess();
    // 开始运行服务
    this->serve();
}

void EpollServer::servePreprocess(){
    this->m_socketFd.reset(new socketFd);   
    this->m_socketFd->bindAndListen(this->m_port);

    // epoll 初始化
    this->m_epollFd.reset(new epollFd(5));
    // 注册针对 socket fd 的 read 事件
    this->m_epollFd->addEvent(this->m_socketFd,epollFd::Read); 
}

void EpollServer::serve(){
    while(true){
        if (this->m_closed.load()){
            LOG_WARN("server closed......");
            return;
        }

        std::vector<epollFd::Event> events = this->m_epollFd->wait();

        LOG_INFO("get epoll event num: %d",events.size());

        for (int i = 0; i < events.size(); i++){
            this->process(events[i]);
        }
    }
}

void EpollServer::process(epollFd::Event& event){
    LOG_INFO("handle epoll event, fd: %d, events: %d",event.fd,event.events);
    
    // 处理到达的客户端连接
    if (event.fd == this->m_socketFd->get()){
        this->processConn();
        return;
    }

    // 处理到达的读请求
    if (event.events & epollFd::Read){
        this->processRead();
    }

    if (event.events & epollFd::Write){
        this->processWrite();
    }
}

void EpollServer::processConn(){
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    while(true){
        // 获取到来的客户端连接
        int connFd = accept(this->m_socketFd->get(),(sockaddr*)&client_addr,&client_addr_len);
        if (connFd <= 0){
            LOG_ERROR("accept err, errno: %d",errno);
            return;
        }

        // 针对到来的连接，注册到对应 read 事件到其中
        this->m_epollFd->addEvent(fd::ptr(new fd(connFd)),epollFd::Read);
    }
}

void EpollServer::processRead(){

}

void EpollServer::processWrite(){
    
}


// 停止
void EpollServer::close(){
    LOG_WARN("server closing......");
    this->m_closed.store(true);
}

}}