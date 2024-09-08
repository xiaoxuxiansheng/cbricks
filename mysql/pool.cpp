#include <thread>

#include "pool.h"
#include "../base/defer.h"
#include "../trace/assert.h"

namespace cbricks{namespace mysql{

/**
 * 析构函数：
 * - 设置关闭标识为 true
 * - 对 cond 进行 signal 操作，保证异步线程至少被唤醒一次
 */
ConnPool::~ConnPool(){
    lock::lockGuard guard(this->m_lock);
    this->m_closed.store(true);
    // 唤醒所有等待连接的线程退出
    this->m_cond.broadcast();
    while (!this->m_pool.empty()){
        Conn* target = this->m_pool.front();
        this->m_pool.pop();
        delete target;
        this->m_activeCnt--;
    }
        
    /**
     * 如果还有连接在外没有回收，则不能析构，需要保证等到所有连接都归还；
     * 如果还有用户在等待连接，则不能析构，需要保证其都正常退出
     */
    while (this->m_activeCnt.load() || this->m_subscribeCnt.load()){
        this->m_closeCond.wait(this->m_lock);
    }

}

/**
 * 初始化数据库连接池：
 * - 设置好各项配置参数
 * - 启动对应于 minSize 数量的 mysql 连接
 * - mysql 连接一一添加到连接池中
 */
void ConnPool::init(
    std::string user,
    std::string passwd, 
    std::string db, 
    std::string ip, 
    const int port, 
    const int minSize, 
    const int maxSize,
    const seconds timeout, 
    const seconds maxIdle)
{
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_initialized.load(),"repeat init");
    CBRICKS_ASSERT(user != "" && passwd != "" && db != "" && ip != "" && port > 0 &&
    minSize > 0 && maxSize > 0 && minSize <= maxSize , "invalid param");

    /**
     * 参数初始化
     */
    this->m_user = user;
    this->m_passwd = passwd;
    this->m_db = db;
    this->m_ip = ip;
    this->m_port = port;
    this->m_maxSize = maxSize;
    this->m_minSize = m_minSize;
    this->m_maxIdle = maxIdle;
    this->m_timeout = timeout;

    /**
     * 启动对应于 minSize 数量的 mysql 连接，并且添加到连接池中
     */
    for (int i = 0; i < minSize; i++){
        this->putConnLocked(this->spawn());
    }

    // 启动异步线程 定期回收空闲连接
    thread thr(std::bind(&ConnPool::recycleIdleConns,this));

    this->m_activeCnt.store(this->m_minSize);
    this->m_initialized.store(true);
}

Conn* ConnPool::spawn(){
    Conn* conn = new Conn;
    bool ret = conn->connect(this->m_user,this->m_passwd,this->m_db,this->m_ip,this->m_port);
    CBRICKS_ASSERT(ret,"conn failed");
    return conn;
}

/**
 * 从连接池中获取可用的连接
 */
Conn::ptr ConnPool::getConn(){
    this->m_subscribeCnt++;
    base::Defer defer([this](){this->m_subscribeCnt--;});

    // 加锁
    lock::lockGuard guard(this->m_lock);
    if (this->m_closed.load()){
        this->m_closeCond.signal();
        return nullptr;
    }

    // 既没有可用连接，且连接数量已经达到上限，则需要陷入阻塞等待中
    while ( this->m_pool.empty() && this->m_activeCnt.load() >= this->m_maxSize ){
        // 如果因为超时而苏醒，则返回空指针，代表获取连接失败
        if (this->m_cond.waitFor(this->m_lock,this->m_timeout)){
            return nullptr;
        }
        if (this->m_closed.load()){
            this->m_closeCond.signal();
            return nullptr;
        }
    }

    // 连接池非空，则弹出连接进行使用即可
    if (!this->m_pool.empty()){
        // 获取连接并返回
        Conn::ptr target(this->m_pool.front(),[this](Conn* conn){
        this->putConn(conn);
    });
        this->m_pool.pop();
        return target;
    }

    // 活跃连接数还没达到 maxSize，则构造出新的连接进行使用
    Conn::ptr target(this->spawn(),[this](Conn* conn){
        this->putConn(conn);
    });
    this->m_activeCnt++;
    return target;
}

/**
 * 连接放回连接池
 */
void ConnPool::putConn(Conn* conn){
    lock::lockGuard guard(this->m_lock);
    /**
     * 如果连接池已经关闭，则直接回收连接
     */
    if (this->m_closed.load()){
        delete conn;
        this->m_closeCond.signal();
        return;
    }

    /**
     * 连接放回池子
     */
    this->putConnLocked(conn);
    /**
     * 通知等待连接的 thread 有连接归还了
     */
    this->m_cond.signal();
}

void ConnPool::putConnLocked(Conn* conn){
    conn->refreshAliveTime();
    this->m_pool.push(conn);
}

/**
 * 定期回收多余的 mysql 连接：
 * - 前提一：连接数量 > minSize
 * - 前提二：连接空闲时长 >= maxIdle
 */
void ConnPool::recycleIdleConns(){
    // main loop
    while (true){
        lock::lockGuard guard(this->m_lock);
        // 连接池已关闭，主动退出
        if (this->m_closed.load()){
            return;
        }

        // 如果池中空闲连接数量 > minSize
        while(this->m_pool.size() > this->m_minSize){
            // 获取队首的连接
            Conn* target = this->m_pool.front();
            // 连接未过期，则 break
            if (!target->isExpired(this->m_maxIdle)){
                break;
            }
            // 释放该连接
            this->m_pool.pop();
            delete target;
            this->m_activeCnt--;
        }

        // 未达到释放连接的条件，睡一秒
        std::this_thread::sleep_for(seconds(1));
    }
}



}}