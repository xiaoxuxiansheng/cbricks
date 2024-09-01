#include "conn.h"
#include "../trace/assert.h"

namespace cbricks{namespace server{

Conn::Conn(int fd):m_fd(fd){}

// 读取全量内容
void Conn::readAll(){
    
}

// 写入内容
void Conn::write(){

}

}}