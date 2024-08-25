#pragma once 

#include <memory>
#include <functional>
#include <ucontext.h>

#include "../base/nocopy.h"

namespace cbricks{ namespace sync{

class Coroutine : base::Noncopyable, std::enable_shared_from_this<Coroutine>{
public:
    // 智能指针类型别名
    typedef std::shared_ptr<Coroutine> ptr;

    enum State{
        // 空闲
        Idle,
        // 终止
        Dead,
        // 可运行
        Runnable,
        // 运行中
        Running,
        // 等待中. 只有 main 协程在等待 worker 协程时，会出现此状态
        Waiting
    };

public:
    // 构造/析构函数 默认栈大小为 64 kb
    Coroutine(std::function<void ()> cb, size_t stackSize = 64 * 1024);
    ~Coroutine();

public:
    // 公有操作函数
    // 获取某个协程的状态
    State getState()const;
    // 获取某个协程的 id
    uint64_t getId()const;
    // 执行工作协程
    void go();
    // 主动让渡
    void sched();

public:
    // 静态公有操作函数
    // 获取当前正在运行的协程实例
    static Coroutine* GetThis();
    // 获取线程下的 main 协程实例
    static Coroutine* GetMain();

    // 协程运行函数 schema，通过 static 风格，隐藏 this 指针
    static void Fc();
    
private:
    // 私有函数
    // 线程下 main 协程对应的构造函数
    Coroutine();
    // 终止协程
    void exit();
    // 让渡协程
    void sched(const bool exit);

private:
    // 协程唯一标识 id
    uint64_t m_id;
    // 协程运行栈大小
    size_t m_stackSize;
    // 协程栈空间地址
    void* m_stack;
    // 协程状态
    Coroutine::State m_state;
    // 基于 c 风格下 ucontext 库关联的协程核心结构
    ucontext_t m_core;
    // 协程执行的函数
    std::function<void()> m_cb;
};

}}