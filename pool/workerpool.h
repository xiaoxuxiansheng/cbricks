#pragma once 

#include <memory>
#include <functional>
#include <atomic>
#include <queue>
#include <unordered_map>

#include "../sync/coroutine.h"
#include "../sync/channel.h"
#include "../sync/thread.h"
#include "../sync/sem.h"
#include "../base/nocopy.h"

namespace cbricks{namespace pool{
// 协程调度池
class WorkerPool : base::Noncopyable{
public:
    // 协程池共享指针类型别名
    typedef std::shared_ptr<WorkerPool> ptr;
    // 协程池中执行的任务
    typedef std::function<void()> task;
    // 一个线程手中持有的本地任务队列
    typedef sync::Channel<task> localq;
    // 本地任务队列智能指针别名
    typedef localq::ptr localqPtr;
    // 线程智能指针别名
    typedef sync::Thread* threadPtr;
    // 一个已分配了运行任务的协程
    typedef sync::Coroutine worker;
    // 协程智能指针别名
    typedef sync::Coroutine::ptr workerPtr;
    // 读写锁别名
    typedef sync::RWLock rwlock;
    typedef sync::Semaphore semaphore;

public:
    // 构造/析构函数
    // threads——使用的线程个数. 默认为 8 个
    WorkerPool(size_t threads = 8);
    // 析构函数
    ~WorkerPool();

public:
    /**
        * 向协程调度池中提交一个任务
        * task 会被随机分配到线程手中，保证负载均衡
        * task 被一个线程取到之后，会创建对应协程实例，为其分配本地栈，此时该任务和协程就固定属于一个线程了
        * 请求参数：taks——提交的一笔任务  nonblock——是否为阻塞模式. 阻塞模式：线程本地任务队列满时阻塞等待 非阻塞模式：线程本地队列满时直接返回 false
        * 返回值：true——提交成功 false——提交失败
    */
    bool submit(task task, bool nonblock = false);

    // 用于在一个工作协程调度任务过程中主动让出调度执行权
    void sched();

private:
    // 协程池调度主函数，持续不断地从本地任务队列 taskq 本地协程队列 t_schedq 中获取调度函数/协程
    void work();
    /**
     * 从指定任务队列中获取任务并执行. 
     * 请求参数：taskq——指定的任务队列 nonblock——是否为阻塞模式
     * bool 类型响应表示是否获取任务成功
    */
    bool readAndGo(localqPtr taskq, bool nonblock);
    // 创建一个协程实例调度某个任务，如果任务未一次性执行完成，会追加线程本地的协程队列 t_schedq 中
    void goTask(task cb);
    // 调度某个协程实例，如果任务未一次性执行完成，会再次追加到线程本地的协程队列 t_schedq 中
    void goWorker(workerPtr worker);

    /**
     * thread——workerPool 中的线程实例
     * - index：线程在线程池中的 index
     * - thr：真正的线程实例，类型为 sync pkg 下的 Thread
     * - taskq：线程的本地任务队列
     * - lock：一把线程粒度的读写锁. 用于隔离往线程池中提交任务的 submit 操作和从其他 thread 中窃取任务补充到该线程的 workstealing 操作
     */
    struct thread{
        typedef std::shared_ptr<thread> ptr;
        int index;
        threadPtr thr;
        rwlock lock;
        localqPtr taskq;
        // 构造函数
        thread(int index,threadPtr thr, localqPtr taskq):index(index),thr(thr),taskq(taskq){}
        ~thread() = default;
    };

    // 从当前线程之外的线程中窃取半数任务填充到当前线程的本地队列中
    void workStealing();
    // 从指定任务队列 stealFrom 中偷取半数任务添加到目标队列 stealTo
    void workStealing(thread::ptr stealTo, thread::ptr stealFrom);
    // 随机获取线程池中除本线程之外的一个线程作为窃取的目标
    thread::ptr getStealingTarget();

    // 通过线程名获取指定线程
    thread::ptr getThreadByThreadName(std::string threadName);
    // 获取当前线程
    thread::ptr getThread();

private:
    // 通过线程 index 转换得到线程名称
    static const std::string getThreadNameByIndex(int index);
    // 获取当前线程对应的 index
    static const int getThreadIndex();
    // 获取当前线程的名称
    static const std::string getThreadName();


private:
    // 线程池，每个元素对应一个线程
    std::vector<thread::ptr> m_threadPool;

    // 标识协程调度池是否已关闭
    std::atomic<bool> m_closed{false};
};

}}