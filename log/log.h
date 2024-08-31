#pragma once 

#include <string>
#include <atomic>
#include <stdio.h>
#include <stdarg.h>

#include "../base/nocopy.h"
#include "../base/defer.h"
#include "../sync/channel.h"
#include "../sync/thread.h"
#include "../sync/sem.h"

namespace cbricks{ namespace log{

/**
 * 基于单例模式实现日志模块
 */
class Logger : base:: Noncopyable {
public:
    typedef sync::Channel<std::string> buffer;
    typedef base::Defer defer;
    typedef sync::Thread thread;
    typedef sync::Lock lock;
    typedef sync::Semaphore semaphore;

public:
    // 日志级别
    enum Level{
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    // 记录日志
    void log(Level level, std::string format, ...);

    // // 重载 << 操作符. 写入日志，info 级别
    void operator<<(std::string info);

public:
    // 静态方法获取单例
    static Logger& GetInstance();

    /**
     * 静态方法初始化日志模块
     * - fileName 日志存放文件名
     * - splitLines 日志文件每写多少行进行拆分
     * - bufferSize 日志数据缓冲池大小，当超过此空间时写入的数据会丢弃
     * - maxLen 单条日志最大长度 单位字符
     */
    static void Init(const char* fileName, const int splitLines = 1000000, int bufferSize = 8192, int maxLen = 8192);

private:
    // 构造函数私有化 实现单例
    Logger() = default;
    // 析构函数私有化 防止误删
    ~Logger();

private:
    // 异步线程持续读取缓冲区 完成日志落盘操作
    static void asyncWriteLog();
    // 异步线程前处理工作，判断是否创建新的日志文件
    static void asyncWriteLogPreprocess();

private:
    // 日志所在目录路径
    char m_dir[128];
    // 日志文件名
    char m_fileName[128];
    // 日志文件每达到多少行数据进行拆分
    int m_splitLines;
    // 写入的单行日志最大字符数限制
    int m_maxLen;
    // 日志文件指针
    FILE* m_file;
    // 记录一个文件中已写入的日志条数
    int m_cnt;

    // 当前日期
    int m_today;

    // 一把互斥锁，用于初始化日志模块时保证并发安全
    lock m_lock;

    // 记录日志模块是否已初始化
    std::atomic<bool> m_initialized{false};

    // 日志数据缓冲区  写入 -> m_buffer -> （异步线程）读取并写入文件
    buffer::ptr m_buffer;

    // 信号量，用于在 logger 析构前等待异步线程完成退出
    semaphore m_sem;

}; 
}}

// 日志宏指令定义
#define LOG_DEBUG(format, ...) cbricks::log::Logger::GetInstance().log(cbricks::log::Logger::DEBUG, format, ##__VA_ARGS__); 
#define LOG_INFO(format, ...) cbricks::log::Logger::GetInstance().log(cbricks::log::Logger::INFO, format, ##__VA_ARGS__); 
#define LOG_WARN(format, ...) cbricks::log::Logger::GetInstance().log(cbricks::log::Logger::WARN, format, ##__VA_ARGS__); 
#define LOG_ERROR(format, ...) cbricks::log::Logger::GetInstance().log(cbricks::log::Logger::ERROR, format, ##__VA_ARGS__);