#include "conn.h"
#include "../trace/assert.h"

namespace cbricks{namespace mysql{
// 构造 析构方法
Conn::Conn():m_conn(mysql_init(nullptr)){
    mysql_set_character_set(this->m_conn,"utf-8");
}

Conn::~Conn(){
    if (this->m_conn){
        mysql_close(this->m_conn);
    }
    
    this->freeResult();
}

// 公有执行方法
// 初始化数据库连接
bool Conn::connect(std::string user, std::string passwd, std::string db, std::string ip, const int port = 3306){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(!this->m_connected.load(),"repeat connect");
    auto ret =  mysql_real_connect(this->m_conn,ip.c_str(),user.c_str(),passwd.c_str(),db.c_str(),port,nullptr,0);
    if (ret){
        this->m_connected.store(true);
    }
    return ret != nullptr;
}

// 执行更新类 sql
bool Conn::exec(std::string sql){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(this->m_connected.load(),"exec without conn connected");
    return mysql_query(this->m_conn,sql.c_str()) != 0;
}

// 执行查询类 sql
bool Conn::query(std::string sql){
    lock::lockGuard guard(this->m_lock);
    CBRICKS_ASSERT(this->m_connected.load(),"exec without conn connected");

    this->freeResult();

    if (mysql_query(this->m_conn,sql.c_str())){
        return false;
    }
    this->m_result = mysql_store_result(this->m_conn);
    return true;
}

// 遍历查询得到的结果集合
bool Conn::next(){
    if (!this->m_result){
        return false;
    }
    this->m_row = mysql_fetch_row(this->m_result);
}

// 得到结果集中的字段值
std::string Conn::value(int index){
    int fieldsCnt = mysql_num_fields(this->m_result);
    if (index < 0 || index >= fieldsCnt){
        return "";
    }
    char* val = this->m_row[index];
    int fieldLen = mysql_fetch_lengths(this->m_result)[index];
    return std::string(val,fieldLen);
}

// 创建事务
bool Conn::begin(){
    mysql_autocommit(this->m_conn,false);
}

// 提交事务
bool Conn::commit(){
    return mysql_commit(this->m_conn);
}

// 回滚事务
bool Conn::rollback(){
    return mysql_rollback(this->m_conn);
}

void Conn::freeResult(){
    if (this->m_result){
        mysql_free_result(this->m_result);
        this->m_result = nullptr;
    }
}

void Conn::refreshAliveTime(){
    this->m_refreshPoint = std::chrono::steady_clock::now();
}

bool Conn::isExpired(seconds idleTimeout){
    seconds duration = std::chrono::duration_cast<std::chrono::seconds>(clock::now() - this->m_refreshPoint);
    return duration >= idleTimeout;
}

}}