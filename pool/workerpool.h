// 保证头文件内容不被重复编译
#pragma once 

/**
 依赖的标准库头文件   
*/
// 标准库智能指针相关
#include <memory>
// 标准库函数编程相关
#include <functional>
// 标准库原子量相关
#include <atomic>
// 标准库——动态数组，以此作为线程池的载体
#include <vector>

/**
    依赖的项目内部头文件
*/
// 线程 thread 实现
#include "../sync/thread.h"
// 协程 coroutine 实现
#include "../sync/coroutine.h"
// 阻塞队列 channel 实现 （一定程度上仿 golang channel 风格）
#include "../sync/channel.h"
// 信号量 semaphore 实现
#include "../sync/sem.h"
// 拷贝禁用工具，用于保证类实例无法被值拷贝和值传递
#include "../base/nocopy.h"

// 命名空间 cbricks::pool
namespace cbricks{namespace pool{
// 协程调度池 继承 Noncopyable 保证禁用值拷贝和值传递功能
class WorkerPool : base::Noncopyable{
public:
    // 协程池共享指针类型别名
    typedef std::shared_ptr<WorkerPool> ptr;
    // 一笔需要执行的任务
    typedef std::function<void()> task;
    // 一个线程持有的本地任务队列
    typedef sync::Channel<task> localq;
    // 本地任务队列指针别名
    typedef localq::ptr localqPtr;
    // 线程指针别名
    typedef sync::Thread* threadPtr;
    // 一个分配了运行任务的协程
    typedef sync::Coroutine worker;
    // 协程智能指针别名
    typedef sync::Coroutine::ptr workerPtr;
    // 读写锁别名
    typedef sync::RWLock rwlock;
    // 信号量类型别名
    typedef sync::Semaphore semaphore;

public:
    /**
      构造/析构函数
    */
    // 构造函数  threads——使用的线程个数. 默认为 8 个
    WorkerPool(size_t threads = 8);
    // 析构函数  
    ~WorkerPool();

public:
    /**
        公开方法
    */
    /**
        * submit: 向协程调度池中提交一个任务 （仿 golang 协程池 ants 风格）
            - task 会被随机分配到线程手中，保证负载均衡
            - task 被一个线程取到之后，会创建对应协程实例，为其分配本地栈，此时该任务和协程就固定属于一个线程了
        * param：task——提交的任务  nonblock——是否为阻塞模式
            - 阻塞模式：线程本地任务队列满时阻塞等待 
            - 非阻塞模式：线程本地队列满时直接返回 false
        * response：true——提交成功 false——提交失败
    */
    bool submit(task task, bool nonblock = false);

    // 工作协程调度任务过程中，可以通过执行次方法主动让出线程的调度权 （仿 golang runtime.Goched 风格）
    void sched();

private:
    /**
     * thread——workerPool 中封装的线程类
     * - index：线程在线程池中的 index
     * - thr：真正的线程实例，类型为 sync/thread.h 中的 Thread
     * - taskq：线程的本地任务队列，其中数据类型为闭包函数 void()
     * - lock：一把线程实例粒度的读写锁. 用于隔离 submit 操作和 workstealing 操作，避免因任务队列阻塞导致死锁
     */
    struct thread{
        typedef std::shared_ptr<thread> ptr;
        int index;
        threadPtr thr;
        localqPtr taskq;
        rwlock lock;
        /**
         *  构造函数
         * param: index: 线程在线程池中的 index; thr: 底层真正的线程实例; taskq：线程持有的本地任务队列
        */ 
        thread(int index,threadPtr thr, localqPtr taskq):index(index),thr(thr),taskq(taskq){}
        ~thread() = default;
    };

private:
    /**
        私有方法
    */
    // work：线程运行主函数，持续不断地从本地任务队列 taskq 或本地协程队列 t_schedq 中获取任务/协程进行调度. 倘若本地任务为空，会尝试从其他线程本地任务队列窃取任务执行
    void work();
    /**
     * readAndGo：从指定的任务队列中获取任务并执行
     * param：taskq——指定的任务队列 nonblock——是否为阻塞模式
     * reponse：true——成功 false——失败
    */
    bool readAndGo(localqPtr taskq, bool nonblock);
    /**
     * goTask: 为一笔任务创建一个协程实例，并调度该任务函数
     * param: cb——待执行任务
     * tip：如果该任务未一次性执行完成（途中使用了 sched 方法），则会在栈中封存好任务的执行信息，然后将该协程实例追加到线程本地的协程队列 t_schedq 中，等待后续再被线程调度
     */
    void goTask(task cb);
    /**
     * goWorker：调度某个协程实例，其中已经分配好执行的任务函数
     * param: worker——分配好执行任务函数的协程实例
     * tip：如果该任务未一次性执行完成（途中使用了 sched 方法），则会在栈中封存好任务的执行信息，然后将该协程实例追加到线程本地的协程队列 t_schedq 中，等待后续再被线程调度
    */ 
    void goWorker(workerPtr worker);

    /**
     * workStealing：当其他线程任务队列 taskq 中窃取半数任务填充到本地队列
     */
    void workStealing();
    /**
     * workStealing 重载：从线程 stealFrom 的任务队列中窃取半数任务填充到线程 stealTo 本地队列
     */
    void workStealing(thread::ptr stealTo, thread::ptr stealFrom);
    /**
     * getStealingTarget：随机获取一个线程作为窃取目标
     */
    thread::ptr getStealingTarget();

    /**
     * getThreadByThreadName 通过线程名称获取对应的线程实例
     */
    thread::ptr getThreadByThreadName(std::string threadName);
    /**
     * getThread 获取当前线程实例
     */
    thread::ptr getThread();

private:
    /**
     * 静态私有方法
     */
    // getThreadNameByIndex：通过线程 index 映射得到线程名称
    static const std::string getThreadNameByIndex(int index);
    // getThreadIndex：获取当前线程的 index
    static const int getThreadIndex();
    // getThreadName：获取当前线程的名称
    static const std::string getThreadName();


private:
    /**
     * 私有成员属性
     */
    // 基于 vector 实现的线程池，元素类型为 WorkerPool::thread 对应共享指针
    std::vector<thread::ptr> m_threadPool;

    // 基于原子变量标识 workerPool 是否已关闭
    std::atomic<bool> m_closed{false};
};

}}