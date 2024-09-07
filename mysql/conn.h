#pragma once 

#include <atomic>
#include <string>
#include <memory>
#include <chrono>
#include <mysql/mysql.h>

#include "../sync/lock.h"
#include "../base/nocopy.h"
#include "pool.h"

namespace cbricks{namespace mysql{

class Conn : base::Noncopyable{
friend class ConnPool;
public:
    typedef std::shared_ptr<Conn> ptr;
    typedef sync::Lock lock;
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::seconds seconds;

public:
    // 构造 析构方法
    Conn();
    ~Conn();

public:
    // 公有执行方法
    // 初始化数据库连接
    bool connect(std::string user, std::string passwd, std::string db, std::string ip, const int port = 3306);
    // 执行更新类 sql
    bool exec(std::string sql);
    // 执行查询类 sql
    bool query(std::string sql);
    // 遍历查询得到的结果集合
    bool next();
    // 得到结果集中的字段值
    std::string value(int index);
    // 创建事务
    bool begin();
    // 提交事务
    bool commit();
    // 回滚事务
    bool rollback();

private:
    void freeResult();
    void refreshAliveTime();
    bool isExpired(seconds idleTimeout);

private:
    // mysql 连接
    MYSQL* m_conn;
    // 结果数据
    MYSQL_RES* m_result;
    MYSQL_ROW m_row;
    // 互斥锁
    lock m_lock;
    // 是否已连接
    std::atomic<bool> m_connected{false};
    clock::time_point m_refreshPoint;
};


}}