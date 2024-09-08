#include "conn.h"
#include "../trace/assert.h"

namespace cbricks{namespace mysql{
/**
 * 构造函数：
 * - 完成 m_conn 初始化
 * - 设定字符集为 utf-8
 */
Conn::Conn():m_conn(mysql_init(nullptr)){
    mysql_set_character_set(this->m_conn,"utf-8");
}

/**
 * 析构函数
 * - 释放 conn
 * - 释放 result
 */
Conn::~Conn(){
    lock::lockGuard guard(this->m_lock);
    this->m_closed.store(true);
    // 建立过连接，则关闭之
    if (this->m_conn){
        mysql_close(this->m_conn);
    }
    // 如果缓存了查询结果，则进行释放
    this->freeResult();
}

// 公有执行方法
// 初始化数据库连接
bool Conn::connect(std::string user, std::string passwd, std::string db, std::string ip, const int port){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"connect with conn closed");
    CBRICKS_ASSERT(this->m_conn == nullptr,"repeat connect");
    return mysql_real_connect(this->m_conn,ip.c_str(),user.c_str(),passwd.c_str(),db.c_str(),port,nullptr,0) != nullptr;
}

// 执行更新类 sql
bool Conn::exec(std::string sql){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"exec with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"exec without conn connected");
    return mysql_query(this->m_conn,sql.c_str()) == 0;
}

// 执行查询类 sql
bool Conn::query(std::string sql){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"query with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"query without conn connected");

    // 针对前一次的结果进行释放
    this->freeResult();

    // 执行 query 
    if (mysql_query(this->m_conn,sql.c_str())){
        return false;
    }

    // 缓存 query 结果
    this->m_result = mysql_store_result(this->m_conn);
    return true;
}

// 遍历查询得到的结果集合
bool Conn::next(){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"iterate with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"iterate without conn connected");
    if (!this->m_result){
        return false;
    }
    this->m_row = mysql_fetch_row(this->m_result);
}

// 得到结果集中的字段值
std::string Conn::value(int index){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"get value with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"get value without conn connected");
    CBRICKS_ASSERT(this->m_result != nullptr,"get value with empty result");

    // 查询结果的列数量
    int fieldsCnt = mysql_num_fields(this->m_result);
    CBRICKS_ASSERT(index >= 0 && index < fieldsCnt,"invalid field index");

    // 返回对应列的结果
    return std::string(this->m_row[index],mysql_fetch_lengths(this->m_result)[index]);
}

// 创建事务
bool Conn::begin(){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"begin with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"begin without conn connected");
    CBRICKS_ASSERT(!this->m_inTx.load(),"repeat begin");

    // 开启事务
    bool ret = mysql_autocommit(this->m_conn,false);
    this->m_inTx.store(ret);
    return ret;
}

// 提交事务
bool Conn::commit(){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"commit with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"commit without conn connected");
    CBRICKS_ASSERT(this->m_inTx.load(),"commit without begin");

    if (!mysql_commit(this->m_conn)){
        return false;
    }

    this->m_inTx.store(false);
    return true;
}

// 回滚事务
bool Conn::rollback(){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_closed.load(),"rollback with conn closed");
    CBRICKS_ASSERT(this->m_conn != nullptr,"rollback without conn connected");
    CBRICKS_ASSERT(this->m_inTx.load(),"rollback without begin");

    if (!mysql_rollback(this->m_conn)){
        return false;
    }

    this->m_inTx.store(false);
    return true;
}

void Conn::freeResult(){
    if (!this->m_result){
        return;
    }
    
    mysql_free_result(this->m_result);
    this->m_result = nullptr;
}

void Conn::refreshAliveTime(){
    this->m_refreshPoint = std::chrono::steady_clock::now();
}

bool Conn::isExpired(seconds idleTimeout){
    seconds duration = std::chrono::duration_cast<std::chrono::seconds>(clock::now() - this->m_refreshPoint);
    return duration >= idleTimeout;
}

}}