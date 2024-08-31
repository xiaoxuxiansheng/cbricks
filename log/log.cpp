#include <string.h>
#include <time.h>

#include "log.h"
#include "../base/time.h"
#include "../trace/assert.h"
#include "../sync/thread.h"

namespace cbricks{namespace log
{

// 析构函数私有化 防止误删
Logger::~Logger(){
    // 主动关闭 buffer
    this->m_buffer->close();

    // 未初始化过，直接退出
    if (!this->m_initialized.load()){
        return;
    }

    // 初始化过，需要等待异步线程退出
    this->m_sem.wait();

    // 未开启过文件
    if (!this->m_file){
        return;
    }

    // 开启过文件，需要关闭文件
    fclose(this->m_file);
}

// 静态方法获取单例
Logger& Logger::GetInstance(){
    // c++ 11 后保证方法内静态局部变量并发安全
    static Logger instance;
    return instance;
}

/**
 * 初始化日志模块
 * - fileName 日志存放文件名
 * - splitLines 日志文件每写多少行进行拆分
 * - bufferSize 日志数据缓冲池大小，当超过此空间时写入的数据会丢弃
 */
void Logger::Init(const char* fileName, const int splitLines, int bufferSize, int maxLen){
    CBRICKS_ASSERT(splitLines > 0 && bufferSize > 0 && maxLen > 0, 
    "logger splitLines or bufferSize or maxLen invalid");

    // 获取实例
    Logger& instance = Logger::GetInstance();
    // 加锁
    lock::lockGuard guard(instance.m_lock);
    // 不允许重复执行初始化操作
    CBRICKS_ASSERT(!instance.m_initialized.load(), "logger repeat init");

    // 更新配置参数
    instance.m_maxLen = maxLen;
    instance.m_splitLines = splitLines;
    instance.m_cnt = 0;

    // 处理时间信息
    time_t t = time(nullptr);
    tm my_tm = *(localtime(&t));
    instance.m_today = my_tm.tm_mday;
    
    // 日志文件名称
    char logFulleName[256] = {0};
    const char *p = strrchr(fileName, '/');

    if (!p){
        snprintf(logFulleName, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, fileName);
    }else{
        strcpy(instance.m_fileName, p + 1);
        strncpy(instance.m_dir, fileName, p - fileName + 1);
        snprintf(logFulleName, 255, "%s%d_%02d_%02d_%s", instance.m_dir, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, instance.m_fileName);
    }

    // 打开日志文件
    instance.m_file = fopen(logFulleName, "a");
    CBRICKS_ASSERT(instance.m_file != nullptr, "open logger file err");

    // 初始化缓冲区 buffer
    instance.m_buffer.reset(new buffer(bufferSize));

    // 启动异步线程
    thread thr(Logger::asyncWriteLog);
    // 走到这里为止都没发生异常，将初始化标识置为 true
    instance.m_initialized.store(true);
}

// 写日志
void Logger::log(Level level, std::string format, ...){
    // logger 模块必须进行过初始化
    CBRICKS_ASSERT(this->m_initialized.load(),"logger is not initialized");

    // 基于日志级别补充前缀
    std::string prefix;
    switch (level)
    {
    case DEBUG:
        prefix = "[debug]:";
        break;
    case INFO:
        prefix = "[info]:";
        break;
    case WARN:
        prefix = "[warn]:";
        break;
    case ERROR:
        prefix = "[error]:";
        break;
    default:
        prefix = "[info]:";
        break;
    }

    // 组装时间信息
    timeval today = base::getTimeOfDay();
    tm my_tm = base::getLocalTime(&today.tv_sec);

    // 完善日志格式以及内容
    va_list valst;
    va_start(valst, format);

    // 填充时间以及前缀
    char tmpBuf[this->m_maxLen];
    int n = snprintf(tmpBuf, this->m_maxLen, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, today.tv_usec, prefix.c_str());
    
    // 拼接日志内容
    int m = vsnprintf(tmpBuf + n, this->m_maxLen - n - 1, format.c_str(), valst);

    // 日志结尾为 \n\0
    tmpBuf[n + m] = '\n';
    tmpBuf[n + m + 1] = '\0';

    // 以非阻塞模式将日志投递到阻塞队列中
    this->m_buffer->write(std::string(tmpBuf),true);

    va_end(valst);
}

// 重载 << 操作符
void Logger::operator<<(std::string info){
    this->log(INFO,info);
}

// 异步线程持续读取缓冲区 完成日志落盘操作
void Logger::asyncWriteLog(){
    // 线程退出前 notify 信号量
    defer d([](){Logger::GetInstance().m_sem.notify();});

    Logger& instance = Logger::GetInstance();
    // 一行日志内容
    std::string logItem;
    // 从 buffer 中取出一条日志数据
    while (instance.m_buffer->read(logItem)){
        // 前处理操作
        instance.asyncWriteLogPreprocess();
        // 将日志数据写入到文件
        fputs(logItem.c_str(),instance.m_file);
        instance.m_cnt++;
    }

    // buffer 已关闭... 退出线程
}

void Logger::asyncWriteLogPreprocess(){
    Logger& instance = Logger::GetInstance();

    if (instance.m_cnt == 0){
        return;
    }  

    // 查看是否要分割文件
    timeval tv = base::getTimeOfDay();
    tm t = base::getLocalTime(&tv.tv_sec);

    // 没有跨天且日志条数达到分割的阈值，则无需分割文件
    if (instance.m_today == t.tm_mday && instance.m_cnt % instance.m_splitLines != 0){
        return;
    }

    // 走到此处意味着需要分割文件
    // 针对老的日志文件落盘并且关闭
    fflush(instance.m_file);
    fclose(instance.m_file);
    
    char newLog[256] = {0};
    char tail[16] = {0};
    // 新日志名称尾部时间格式拼接
    snprintf(tail, 16, "%d_%02d_%02d_", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    
    // 如果跨天了
    if (instance.m_today != t.tm_mday){
        snprintf(newLog, 255, "%s%s%s", instance.m_dir, tail, instance.m_fileName);
        instance.m_today = t.tm_mday;
        instance.m_cnt = 0;
    }else{
        // 未跨天，是因为达到日志文件分割阈值
        snprintf(newLog, 255, "%s%s%s.%lld", instance.m_dir, tail, instance.m_fileName, instance.m_cnt / instance.m_splitLines);
    }
    
    // 开启新的日志文件
    instance.m_file = fopen(newLog, "a");
}

}}