#pragma once 

#include <memory>
#include <functional>
#include <atomic>
#include <queue>
#include <unordered_map>

#include "../sync/coroutine.h"
#include "../sync/channel.h"
#include "../sync/thread.h"
#include "../base/nocopy.h"

namespace cbricks{namespace pool{

class WorkerPool : base::Noncopyable{
public:
    typedef std::shared_ptr<WorkerPool> ptr;
    typedef std::function<void()> task;
    typedef sync::Channel<task> localq;
    typedef localq::ptr localqPtr;
    typedef sync::Coroutine worker;
    typedef sync::Coroutine::ptr workerPtr;

public:
    // 构造/析构函数
    // threads——使用的线程个数. 默认为 8 个
    WorkerPool(size_t threads = 8);
    ~WorkerPool();

public:
    // 提交任务
    // cb 会被随机分配到线程手中，在任务分配之前可以负载均衡
    // item 被一个线程取到之后，就属于一个线程了
    bool submit(task task);

private:
    // 协程池调度函数
    void work();
    // 通过 index 转换得到线程名称
    std::string getThreadNameByIndex(int index);
    // 从某个线程本地队列中获取任务并执行. nonblock 标识获取方式是否为阻塞等待. bool 类型响应表示是否读取成功
    bool readAndGo(localqPtr taskq, bool nonblock);
    // 创建一个协程实例调度某个任务，如果未执行完成会添加到线程本地的协程队列中
    void goTask(task cb);
    void goWorker(workerPtr worker);

private:
    // 一个线程下的任务队列
    localqPtr getLocalQueue();
    localqPtr getLocalQueueByThreadName(std::string threadName);

private:
    // 一个 map 着线程到本地任务队列之间的映射关系
    // 每个线程会有两项内容，一项是本地分配的函数 list
    // 另一项是已经创建好的协程，但是因为主动让渡，而进入了协程队列
    // 已经创建过的协程一定已经明确只能从属于某个线程了，所以这部分直接放在 thread_local 中即可
    std::unordered_map<std::string,localqPtr> m_taskQueues;

    // 线程池
    std::vector<sync::Thread::ptr> m_threadPool;

    // 池子是否已关闭
    std::atomic<bool> m_closed{false};
};

}}