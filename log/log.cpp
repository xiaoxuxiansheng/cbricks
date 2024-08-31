#include <string.h>
#include <time.h>

#include "log.h"
#include "../trace/assert.h"
#include "../sync/thread.h"

namespace cbricks{namespace log
{

// 析构函数私有化 防止误删
Logger::~Logger(){
    // 主动关闭 buffer
    this->m_buffer->close();
    // 如果初始化过，则需要等待异步线程退出
    if (this->m_initialized.load()){
        // 等待异步线程退出
        this->m_sem.wait();
        // 关闭文件
        fclose(this->m_file);
    }
}

// 静态方法获取单例
Logger* Logger::GetInstance(){
    // c++ 11 后保证方法内静态局部变量并发安全
    static Logger* instance = new Logger;
    return instance;
}

/**
 * 初始化日志模块
 * - fileName 日志存放文件名
 * - splitLines 日志文件每写多少行进行拆分
 * - bufferSize 日志数据缓冲池大小，当超过此空间时写入的数据会丢弃
 */
bool Logger::init(const char* fileName, const int splitLines, int bufferSize){
    CBRICKS_ASSERT(splitLines > 0 && bufferSize > 0, "logger splitLines or bufferSize splitLines invalid");
    
    this->m_splitLines = splitLines;

    // 处理时间信息
    time_t t = time(nullptr);
    tm my_tm = *(localtime(&t));
    this->m_today = my_tm.tm_mday;
 
    char logFulleName[256] = {0};
    const char *p = strrchr(fileName, '/');

    if (!p)
    {
        snprintf(logFulleName, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, fileName);
    }
    else
    {
        strcpy(this->m_fileName, p + 1);
        strncpy(this->m_dir, fileName, p - fileName + 1);
        snprintf(logFulleName, 255, "%s%d_%02d_%02d_%s", this->m_dir, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, this->m_fileName);
    }

    // 打开日志文件
    this->m_file = fopen(logFulleName, "a");
    if (!this->m_file)
    {
        return false;
    }    

    // 初始化缓冲区
    this->m_buffer.reset(new buffer(bufferSize));
    // 启动线程
    thread thr([this](){
        defer d([this](){this->m_sem.notify();});
        this->asyncWriteLog();
    });
    // 走到这里为止都没发生异常，将初始化标识置为 true
    this->m_initialized.store(true);
}

// 写日志
void Logger::log(Level level, std::string format, ...){

}

// 异步线程持续读取缓冲区 完成日志落盘操作
void Logger::asyncWriteLog(){
    std::string logItem;
    // 从 buffer 中取出一条日志数据
    while (this->m_buffer->read(logItem)){
        // 保证并发安全
        lock::lockGuard guard(this->m_lock);
        // 将日志数据写入到文件
        fputs(logItem.c_str(),this->m_file);
    }

    // buffer 已关闭，日志模块已析构
}

/**
 * 断言日志模块已经完成过初始化
 */
void Logger::assertInitalized(){
    CBRICKS_ASSERT(this->m_initialized,"logger is not initialized")
}

}}