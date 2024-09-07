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
    ConnPool();
    ~ConnPool();

public:
    // 对 mysql 数据库初始化
    void init(std::string user, std::string passwd, std::string db, std::string ip, const int port = 3306, const int minSize = 4, const int maxSize = 16, const seconds timeout = seconds(2), const seconds maxIdle = seconds(1));
    // 从连接池中获取连接
    Conn::ptr getConn();

private:
    void recycleIdleConns();

private:
    // 连接配置参数
    std::string m_user;
    std::string m_passwd;
    std::string m_db;
    std::string m_ip;
    int m_port;
    // 最大连接个数
    int m_maxSize;
    // 最少连接个数
    int m_minSize;
    // 阻塞等待超时阈值，单位秒
    Conn::seconds m_timeout;
    // 空闲连接最大等待时长，单位秒
    Conn::seconds m_maxIdle;

    // 连接池
    std::queue<Conn::ptr> m_pool;

    // 并发控制
    lock m_lock;
    cond m_cond;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_closed{false};
};

}}