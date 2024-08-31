#pragma once 

#include <string>
#include <atomic>
#include <stdio.h>

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
    typedef sync::Semaphore semaphore;
    typedef base::Defer defer;
    typedef sync::Thread thread;
    typedef sync::Lock lock;

public:
    // 日志级别
    enum Level{
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    /**
     * 初始化日志模块
     * - fileName 日志存放文件名
     * - splitLines 日志文件每写多少行进行拆分
     * - bufferSize 日志数据缓冲池大小，当超过此空间时写入的数据会丢弃
     * - 响应参数：true——初始化成功 false——初始化失败
     */
    bool init(const char* fileName, const int splitLines = 1000000, int bufferSize = 8192);

    // 写日志
    void log(Level level, std::string format, ...);

public:
    // 静态方法获取单例
    static Logger* GetInstance();

private:
    // 构造函数私有化 实现单例
    Logger() = default;
    // 析构函数私有化 防止误删
    ~Logger();

    // 异步线程持续读取缓冲区 完成日志落盘操作
    void asyncWriteLog();

private:
    /**
     * 断言日志模块已经完成过初始化
     */
    void assertInitalized();

private:
    // 日志所在目录路径
    char* m_dir;
    // 日志文件名
    char* m_fileName;
    // 日志文件每达到多少行数据进行拆分
    int m_splitLines;
    // 日志文件指针
    FILE* m_file;

    // 当前日期
    int m_today;

    // 一把互斥锁，用于写入文件时保证并发安全
    lock m_lock;

    // 是否已完成初始化
    std::atomic<bool> m_initialized{false};

    // 日志数据缓冲区
    buffer::ptr m_buffer;

    // 信号量，用于在 logger 析构前等待异步线程完成退出
    semaphore m_sem;
};


}
}