#pragma once 

#include <memory>
#include <functional>
#include <string>
#include <pthread.h>

#include "../base/nocopy.h"

namespace cbricks{ namespace sync{

class Thread : base::Noncopyable, std::enable_shared_from_this<Thread>{

public: 
    // 共享智能指针类型别名
    typedef std::shared_ptr<Thread> ptr;

public:
    // 构造/析构函数
    // cb——线程执行的函数 name——线程名称
    Thread(std::function<void()> cb, const std::string name = "default");
    ~Thread();

public:
    // 公有操作函数
    // 获取线程 id
    pid_t getId() const;

    // 获取线程名称
    const std::string& getName() const;

    // 等待线程执行完成
    void join();

public:
    // 静态操作方法
    // 获取当前线程
    static Thread* GetThis();

private:
    // 线程运行函数 schema，通过 static 风格，隐藏 this 指针
    static void* Fc (void* arg);

private:
    // 私有成员变量
    // 真实的线程 id
    pid_t m_id;
    std::string m_name;
    // 对应 c 风格 pthread 结构
    pthread_t m_core;
    // 用户注入的线程执行函数
    std::function<void()> m_cb;
};

}}