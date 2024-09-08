#pragma once 

#include <atomic>
#include <memory>
#include <string>
#include <queue>
#include <chrono>

#include "conn.h"
#include "../sync/thread.h"
#include "../sync/cond.h"
#include "../sync/lock.h"
#include "../base/nocopy.h"

namespace cbricks{namespace mysql{

// mysql 数据库连接池
class ConnPool : base::Noncopyable{
public:
    typedef std::shared_ptr<ConnPool> ptr;
    typedef sync::Lock lock;
    typedef sync::Cond cond;
    typedef sync::Thread thread;
    typedef std::chrono::seconds seconds;

public:
    // 构造/析构
    ConnPool() = default;
    ~ConnPool();

public:
    /** 
     * 对 mysql 数据库初始化. 只允许执行一次
    */
    void init(std::string user, std::string passwd, std::string db, std::string ip, const int port = 3306, const int minSize = 4, const int maxSize = 16, const seconds timeout = seconds(2), const seconds maxIdle = seconds(1));

    /**
     * 从连接池中获取可用的连接，如果连接不足时，最长等待 timeout 秒
     * reponse：nullptr——获取失败（等待超时） 非 null——获取成功
     */
    Conn::ptr getConn();

private:
    /**
     * 生成新的连接
     */
    Conn* spawn();
    /** 
     * 将连接返还连接池
    */ 
    void putConn(Conn* conn);
    /** 
     * 将连接返还连接池(加锁版)
    */ 
    void putConnLocked(Conn* conn);
    /**
     * 异步线程执行方法，持续探索连接池中空闲连接情况
     * 如果空闲连接数 > m_minSize 并且连接空闲时长 > maxIdle，则会释放对应连接
     */
    void recycleIdleConns();

private:
    // 数据库连接配置
    std::string m_user;
    std::string m_passwd;
    std::string m_db;
    std::string m_ip;
    int m_port;

    // 最大/最小连接个数
    int m_maxSize;
    int m_minSize;

    // 阻塞等待超时阈值，单位秒
    Conn::seconds m_timeout;
    // 空闲连接最大等待时长，单位秒
    Conn::seconds m_maxIdle;

    // 连接池
    std::queue<Conn*> m_pool;

    // 并发控制
    lock m_lock;
    cond m_cond;

    // 用于关闭连接池的 cond
    cond m_closeCond;

    // 活跃连接数
    std::atomic<int> m_activeCnt{0};
    // 等待连接的用户数
    std::atomic<int> m_subscribeCnt{0};
    // 连接池是否已完成过初始化
    std::atomic<bool> m_initialized{false};
    // 连接池是否已关闭
    std::atomic<bool> m_closed{false};
};

}}