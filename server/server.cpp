#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include "server.h"
#include "../trace/assert.h"
#include "../log/log.h"

namespace cbricks{namespace server{ 

/**
 *  类静态变量声明
 *     - s_once: 单例工具
 *     - s_pipe: 管道 fd
 */
Server::once Server::s_once;
Server::pipe::ptr Server::s_pipe;

// 析构函数
Server::~Server(){
    LOG_WARN("server closing...");
    // 一一移除所有的 conn. 移除时会自动调用其析构函数，将其从 epoll 事件表中移除并且关闭
    lock::lockGuard guard(this->m_lock);
    this->m_conns.clear();
    // server 运行过，则需要从 epoll 事件表中移除 pipe
    if (this->m_serving.load()){
        this->m_epoll->remove(Server::s_pipe->getRecvFd()->get());
    }
}

// 初始化 server
void Server::init(int port, callback cb, const int threads, const int maxRequest){
    // 参数合法性校验
    CBRICKS_ASSERT(port > 0, "port must be positive");
    CBRICKS_ASSERT(cb != nullptr, "callback function must be assigned");
    CBRICKS_ASSERT(threads > 0, "threads must be positive");
    CBRICKS_ASSERT(maxRequest > 0, "maxRequest must be positive");
    
    // 加锁后校验是否重复执行过 init 操作
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_initialized.load(), "repeat initialized");

    // 完成参数赋值
    this->m_cb = cb;
    this->m_port = port;
    this->m_maxRequest = maxRequest;
    this->m_workerPool = std::make_shared<workerPool>(threads);

    // 设置 init 完成标识
    this->m_initialized.store(true);
}

// 启动 server
void Server::serve(){
    {
        lock::lockGuard guard(this->m_lock);
        // 确保已完成过初始化操作
        CBRICKS_ASSERT(this->m_initialized.load(), "serve without initialize");
        // 确保未重复执行启动操作
        CBRICKS_ASSERT(!this->m_serving.load(), "repeat serve");
        // 设置 server 启动标识
        this->m_serving.store(true);
    } 

    /**
     * @brief: 前处理：
     * - 1）创建 socketFd (socket 操作)
     * - 2）创建 epollFd （epoll_create 操作）
     * - 3）socketFd 添加到 epollFd 中（epoll_ctl 操作）
     * - 4）创建 pipeFd （socketPair）——全局只会执行一次
     * - 5）pipeFd 读取端添加到 epollF 中（epoll_ctl 操作）
     */
    this->prepare();

    // 运行 server（event loop + epoll_wait + dispatch 操作）
    this->serving();
}

// 运行前处理
void Server::prepare(){
    // 初始化 pipe. 全局只会执行一次
    Server::pipePrepare();

    // 初始化 epoll Fd 
    this->m_epoll.reset(new epoll(this->m_maxRequest));
    // 向 epoll 池中注册监听 pipe 读取端的读就绪事件，需要注意 pipe fd 注册事件不为 oneshot 模式
    this->m_epoll->add(Server::s_pipe->getRecvFd(), epoll::Read, false);

    // 初始化 socket Fd，并绑定监听端口
    socket* s = new socket;
    s->bindAndListen(this->m_port);
    this->m_socket.reset(s);

    // 注册监听针对 socket 的读就绪事件
    this->m_epoll->add(this->m_socket,epoll::Read); 
}

// 运行 server
void Server::serving(){
    // event loop + io 多路复用模型
    while(true){
        // 阻塞等待监听，直到 epoll 事件表中有就绪事件到达
        std::vector<event::ptr> es = this->m_epoll->wait();

        if (!es.size() && errno != EINTR){
            LOG_ERROR("epoll fail, errno: %d",errno);
            return;
        }

        // 分发处理到达的就绪事件
        for (int i = 0; i < es.size(); i++){
            bool pass = this->process(es[i]);
            if (!pass){
                return;
            }
        }
    }
}

/**
 * @brief:分发处理到达的事件：
 *    1) 异常错误
 *    2）到达的客户端连接
 *    3）conn fd 读操作就绪
 *    4）conn fd 写操作就绪
 */
bool Server::process(event::ptr e){
    // 处理信号
    if (e->fd == Server::s_pipe->getRecvFd()->get()){
        return (!e->readable()) || this->processSignal();
    }

    // socket Fd 发生错误，直接退出程序
    if (e->hupOrErr() && e->fd == this->m_socket->get()){
        LOG_ERROR("socket err, events: %d",e->events);
        CBRICKS_ASSERT(false, "socket fd error");
        return false;
    }

    // conn Fd 发生错误，则移除 conn，但不终止整个 server
    if (e->hupOrErr()){
        this->freeConn(e->fd);
        return true;
    }

    // 客户端连接到达
    if (e->fd == this->m_socket->get()){
        this->spawnConn();
        return true;
    }

    // conn 读操作就绪
    if (e->readable()){
        this->processRead(e->fd);
    }

    // conn 写操作就绪
    if (e->writable()){
        this->processWrite(e->fd);
    }

    return true;
}

// 处理信号量. 返回值 true——通过 false——退出 server
bool Server::processSignal(){
    // 缓存接收的信号量
    char signals[1024];
    int sigNum = recv(Server::s_pipe->getRecvFd()->get(), signals, sizeof(signals), 0);
    if (sigNum <= 0){
        return true;
    }

    for (int i = 0; i < sigNum; ++i){
        // 接收到 sigint 或 sigterm 信号 需要退出
        if (signals[i] == SIGINT || signals[i] == SIGTERM){
            LOG_WARN("capture exit signal: %d",signals[i]);
            return false;
        }
    }
    
    return true;
}

/**
 * spawnConn: 处理到达的客户端连接
 */
void Server::spawnConn(){
    // 处理连接需要同步执行，保证在执行新一轮 epoll_wait 前将 fd 添加到 epoll 事件表中
    /**
    * 退出前再次针对 socket fd 注册监听读就绪事件以及 oneshot 事件
    */
    defer d([this](){
        this->m_epoll->modify(this->m_socket, epoll::Read);
    });

    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    /**
    * et 模式，一次性处理所有到达的客户端连接
    */
    while(true){
        // 获取到达的客户端连接
        int connFd = accept(this->m_socket->get(), (sockaddr*)&client_addr, &client_addr_len);
        if (connFd <= 0){
            if (errno != EAGAIN){
                LOG_ERROR("accept err, errno: %d",errno);
            }   
            return;
        }

        // 针对到来的 conn，重载对应的析构函数，实现 conn 析构时自动从 epoll 事件表中移除
        conn::ptr _conn(new conn(connFd),[this](conn* connFd){
            this->m_epoll->remove(connFd->get());
            connFd->~ConnFd();
        });

        lock::lockGuard guard(this->m_lock);
        // conn 实例添加进入 conns 池中，防止析构
        this->m_conns.insert({connFd,_conn});
        // 在 epoll 事件表中注册监听 conn 的读就绪事件
        this->m_epoll->add(_conn,epoll::Read);
    }
}

/**
 * @brief:读操作就绪时执行该函数：
 *  1 从 fd 中读取全量数据写入到 conn 读缓冲区
 *  2 从 conn 读缓冲区中获取全量数据
 *  3 调用 callback 函数执行业务逻辑
 *  4 获取业务逻辑返回的响应数据，写入 conn 写缓冲区
 *  5 注册监听 conn 对应的写操作就绪事件
 */
// 读操作就绪时执行该
void Server::processRead(int _fd){
    // 调用 callback 函数，获取到结果后将其写回到 conn 中即可
    semaphore sem;
    // 读取数据后写回到 conn 中
    this->m_workerPool->submit([this,&_fd,&sem](){
        /**
         * 1 获取对应 conn
         */
        conn::ptr connFd = this->getConn(_fd);
        /**
         * 2 放行外层函数
         */
        sem.notify();
        /**
         * 3 从 conn fd 中读取数据，写入到读缓冲区
         */
        connFd->readFd();
        /**
         * 4 获取 conn 读缓冲区数据
         */
        std::string& requestBody = connFd->readFromBuf();
        /**
         * 5 执行用户注入的 callback 函数
         */
        std::string responseBody = this->m_cb(requestBody);
        /**
         * 6 执行结果写入到 conn 写缓冲区
         */
        connFd->writeToBuf(responseBody);
        /**
         * 7 注册监听 conn 写就绪事件
         */
        this->m_epoll->modify(connFd, epoll::Write);
    });

    sem.wait();
}

/**
 * @brief:写操作就绪后执行相应写动作：
 *    1 将 conn 写缓冲区中数据写入 fd
 *    2 释放对应的 conn
 */
void Server::processWrite(int _fd){
    semaphore sem;
    // 异步执行，通过 sem 保证 conn 获取成功后再退出外层函数
    this->m_workerPool->submit([this,&_fd,&sem](){
        /**
         * 1 获取对应 conn
         */
        conn::ptr connFd = this->getConn(_fd);
        /**
         * 2 放行外层函数
         */
        sem.notify();
        /**
         * 3 将 conn 写缓冲区中的数据写入 fd
         */
        connFd->writeFd();
        /**
         * 4 回收对应的 conn 
         */
        this->freeConn(connFd->get());
    });
    sem.wait();    
}

/**
 * 根据 fd 从 conn 池中获取指定 conn 
 */
Server::conn::ptr Server::getConn(int _fd){
    lock::lockGuard guard(this->m_lock);
    auto it = this->m_conns.find(_fd);
    CBRICKS_ASSERT(it != this->m_conns.end(),"invalid fd not found conn in polls");
    return it->second;
}

// 从 conn 池中移除对应 conn 即可触发其析构，完成从 epoll 池中的移除操作
void Server::freeConn(int _fd){
    lock::lockGuard guard(this->m_lock);
    this->m_conns.erase(_fd);
}

void Server::pipePrepare(){
    // 全局初始化一次 pipe Fd
    Server::s_once.onceDo([](){
        // 初始化 pipe fd
        Server::s_pipe.reset(new pipe);
        // 设置为非阻塞模式
        Server::s_pipe->setNonblocking();
        base::AddSignal(SIGPIPE,SIG_IGN);
        // 注册针对于 pipe 发送端的信号监听事件
        base::AddSignal(SIGTERM,[](int sig){
            Server::s_pipe->sendSig(sig);
        });
        base::AddSignal(SIGINT,[](int sig){
            Server::s_pipe->sendSig(sig);
        });
    }); 
}

}}