#include <atomic>
#include <exception>

#include "coroutine.h"
#include "once.h"
#include "../base/defer.h"

namespace cbricks{namespace sync{

// 全局单调递增的协程 id 生成器
static std::atomic<uint64_t> s_coroutineId{0};

// 当前线程下运行的协程实例
static thread_local Coroutine* t_curWorker = nullptr;
static thread_local Coroutine* t_mainWorker = nullptr;

// 每个线程唯一的单例工具
static thread_local Once t_once;

// main 协程使用的构造函数. 通过线程单例工具保证一个线程内只会针对 main 协程执行一次
Coroutine::Coroutine(){
    // main 协程直接置为运行中
    this->m_state = Coroutine::Running;
    // main 协程的 id 为 0
    this->m_id = 0;

    // main 协程运行中，thread_local 中的 main 协程和当前工作协程都指向 main 协程
    t_mainWorker = this;
    t_curWorker = this;
    
    // 初始化 main 协程对应的协程结构
    if (getcontext(&this->m_core)){
        throw std::exception();
    }
}

// 普通工作协程执行此构造函数
Coroutine::Coroutine(std::function<void ()> cb, size_t stackSize)
    :m_id(++s_coroutineId),
    m_cb(cb),
    m_stackSize(stackSize),
    m_state(Coroutine::Idle)
{
    // 栈空间大小必须为正数
    if (this->m_stackSize <= 0){
        throw std::exception();
    }

    // 懒处理机制，通过 once 工具保证全局只执行一次 main 协程的初始化
    t_once.onceDo([](){Coroutine main;});

    // 为工作协程分配栈空间
    this->m_stack = malloc(this->m_stackSize);

    // 初始化工作协程结构体实例
    if (getcontext(&this->m_core)){
        throw std::exception();
    }
    
    // 为协程结构体实例分配栈信息
    this->m_core.uc_link = nullptr;
    this->m_core.uc_stack.ss_sp = this->m_stack;
    this->m_core.uc_stack.ss_size = this->m_stackSize;

    // 为协程结构体实例绑定执行函数
    makecontext(&this->m_core,&Coroutine::Fc,0);

    // 设置工作协程为可执行状态
    this->m_state = Coroutine::Runnable;
}

// 析构函数，回收协程对应的栈空间. 
Coroutine::~Coroutine(){
    if (this == Coroutine::GetMain()){
        return;
    }
    // 只针对工作协程回收对应的栈空间
    free(this->m_stack);
}

// 获取某个协程的状态
Coroutine::State Coroutine::getState()const{
    return this->m_state;
}

// 获取某个协程的 id
uint64_t Coroutine::getId()const{
    return this->m_id;
}

// 获取当前正在运行的协程实例
Coroutine* Coroutine::GetThis(){
    return t_curWorker;
}

// 获取线程下的 main 协程实例
Coroutine* Coroutine::GetMain(){
    return t_mainWorker;
}

// 工作协程运行函数 schema，通过 static 风格，隐藏 this 指针
void Coroutine::Fc(){  
    // 通过 defer 保证工作协程的 exit 方法被执行
    base::Defer defer([](){
        Coroutine::GetThis()->exit();   
    });

    try{
        std::function<void()> cb = Coroutine::GetThis()->m_cb;
        // 执行工作协程中注册的闭包函数
        if (cb){
            cb();
        }
    }
    catch(...)
    {

    }
}
    
// 执行某个协程协程
void Coroutine::go(){
    // 如果该工作协程非可运行状态，则不作运行
    if (this->m_state != Coroutine::Runnable){
        return;
    }

    // 到这里为止还是线程还是在调度协程之前的主协程. 但是持有的 this 实例是拟调度的工作协程
    // 设置协程为运行中状态
    this->m_state = Coroutine::Running;
    // 设置当前协程运行的协程为工作协程
    t_curWorker = this;

    // 将 main 协程的状态由 running 置为 waiting（等待工作协程执行完成）
    Coroutine::GetMain()->m_state = Coroutine::Waiting;

    // 该方法一旦执行后，就会从主协程切换至工作协程执行，执行入口为 Fc 函数
    if (swapcontext(&Coroutine::GetMain()->m_core,&this->m_core)){
        throw std::exception();
    }
}

// 一个工作协程进行主动让渡，返回 main 协程
void Coroutine::sched(){
    // main 协程不可让渡
    if (this == Coroutine::GetMain()){
        return;
    }

    // 工作协程非执行中状态，不可让渡
    if (this->m_state != Coroutine::Running){
        return;
    }

    // 让渡工作协程，切换回到 main 协程执行
    this->sched(false);
}

// exit——true：工作协程执行终止 exit——false：工作协程主动让渡
void Coroutine::sched(const bool exit){
    // 设置当前工作协程为 main 协程
    Coroutine* mainWorker = Coroutine::GetMain();
    t_curWorker = mainWorker;
    // main 协程状态置为 running 
    mainWorker->m_state = Coroutine::Running;

    // 更新工作协程状态
    if (exit){
        this->m_state = Coroutine::Dead;
    }else{
        this->m_state = Coroutine::Runnable;
    }

    // 切回到 main 协程执行
    if (swapcontext(&this->m_core,&mainWorker->m_core)){
        throw std::exception();
    }
}

// 终止某个 worker 协程
void Coroutine::exit(){
    // main 协程不可通过该方法终止
    if (this == Coroutine::GetMain()){
        return;
    }
    
    this->sched(true);
}

}}