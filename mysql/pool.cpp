#include "pool.h"
#include "../trace/assert.h"

namespace cbricks{namespace mysql{
// 构造/析构
ConnPool::ConnPool(){

}

ConnPool::~ConnPool(){
    lock::lockGuard guard(this->m_lock);
    this->m_closed.store(true);
    this->m_cond.broadcast();
}

void ConnPool::init(std::string user, std::string passwd, std::string db, std::string ip, const int port, const int minSize, const int maxSize, const seconds timeout, const seconds maxIdle){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_initialized.load(),"repeat load");
    CBRICKS_ASSERT(user != "" && passwd != "" && db != "" && ip != "" && port > 0 &&
    minSize > 0 && maxSize > 0 && minSize <= maxSize , "invalid param");
    this->m_user = user;
    this->m_passwd = passwd;
    this->m_db = db;
    this->m_ip = ip;
    this->m_port = port;
    this->m_maxSize = maxSize;
    this->m_minSize = m_minSize;
    this->m_maxIdle = maxIdle;
    this->m_timeout = timeout;

    for (int i = 0; i < minSize; i++){
        Conn::ptr conn(new Conn);
        bool ret = conn->connect(this->m_user,this->m_passwd,this->m_db,this->m_ip,this->m_port);
        CBRICKS_ASSERT(ret,"conn failed");
        conn->refreshAliveTime();
        this->m_pool.push(conn);
    }

    // 启动异步线程 定期回收空闲连接
    thread thr(std::bind(&ConnPool::recycleIdleConns,this));

    // 异步启动线程，定期回收空闲的连接
    this->m_initialized.store(true);
}

void ConnPool::recycleIdleConns(){
    while (true){
        lock::lockGuard guard(this->m_lock);

        if (this->m_closed.load()){
            return;
        }

        while(this->m_pool.size() > this->m_minSize){
            Conn::ptr target = this->m_pool.front();
            if (!target->isExpired(this->m_maxIdle)){
                break;
            }
            this->m_pool.pop();
        }

        this->m_cond.wait(this->m_lock);
    }
}

Conn::ptr ConnPool::getConn(){
    lock::lockGuard guard(this->m_lock);
    
}


}}