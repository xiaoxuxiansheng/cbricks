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

// 一笔 mysql 连接. 不可值拷贝和值传递
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
    /**
     * 公有方法
     */
    /**
     * 连接数据库，全局只可执行一次
     * params：user——用户名 passwd——密码 db——数据库名 ip——地址 port——端口
     */
    bool connect(std::string user, std::string passwd, std::string db, std::string ip, const int port = 3306);
    /**
     *  执行 sql，非查询类
     * params：拟执行的 sql
     * response：true——执行成功 false——执行失败
    */ 
    bool exec(std::string sql);
    /**
     *  执行 sql，非查询类
     * params：拟执行的 sql
     * response：true——执行成功 false——执行失败
    */ 
    bool query(std::string sql);
    /**
     * next 遍历查询得到的数据结果
     */
    bool next();
    /**
     * 返回某一行中的某一列结果
     */
    std::string value(int index);
    /**
     * begin: 启动事务
     * response：true——成功 false——失败
    */ 
    bool begin();
    /**
     * commit：提交事务
     * reponse：true——成功 false——失败
     */
    bool commit();
    /**
     * rollback: 回滚事务
     * reponse：true——成功 false——失败
     */
    bool rollback();

private:
    void freeResult();
    /**
     * refreshAliveTime：刷新连接存活的时间
     */
    void refreshAliveTime();
    /**
     * isExpired： 判断连接是否已长于闲置时长
     */
    bool isExpired(seconds idleTimeout);

private:
    // 真正的 mysql 连接
    MYSQL* m_conn;
    // 数据结果缓存
    MYSQL_RES* m_result;
    // 一行数据
    MYSQL_ROW m_row;
    // 互斥锁，保证并发安全
    lock m_lock;
    // 标识连接是否已关闭
    std::atomic<bool> m_closed{false};
    // 标识是否在事务执行过程中
    std::atomic<bool> m_inTx{false};
    // 建立连接刷新的存活时间点
    clock::time_point m_refreshPoint;
};


}}