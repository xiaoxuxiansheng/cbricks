#include <exception>
#include <queue>

#include "copool.h"

namespace cbricks{ namespace pool{

// 任务 id 计数器
static std::atomic<int> s_taskId{0};

// 线程私有的协程 list
static thread_local std::queue<sync::Coroutine::ptr> t_schedq;

WorkerPool::WorkerPool(size_t threads){
    if (threads <= 0){
        throw std::exception();
    }

    // 初始化好线程池中的每个线程
    this->m_threadPool.reserve(threads);
    for (int i = 0; i < this->m_threadPool.size();i++){
        // 线程名称
        std::string threadName = this->getThreadNameByIndex(i);
        // 线程对应的本地队列实例插入到 map 中
        this->m_taskQueues.insert({threadName,localq(new sync::Queue<task>())});
        // 启动线程实例
        sync::Thread::ptr worker(new sync::Thread(std::bind(&WorkerPool::work,this)),threadName);
        this->m_threadPool.push_back(worker);
    }
}

WorkerPool::~WorkerPool(){
    this->m_closed = true;
    // 等待剩余任务都执行完成
    // 等待所有线程都关闭
    for (int i = 0; i < this->m_threadPool.size(); i++){
        this->m_threadPool[i]->join();
    }
}

// 某个线程持续运行的调度主流程
void WorkerPool::work(){
    // 获取到线程对应的本地队列
    localq taskq = this->getLocalQueue();

    while (true){
        // 防止饥饿，最多调度 10 次的 localq 的情况下，需要看一次 t_schedq
        // 本地队列有任务的情况下，优先处理本地队列
        for (int i = 0; i < 10; i++){
            task cb;
            if (!taskq->pop(cb)){
                break;
            }

            sync::Coroutine::ptr worker(new sync::Coroutine(cb));
            worker->go();

            // 任务执行完成，进入下一个循环
            if (worker->getState() == sync::Coroutine::Dead){
                continue;
            } 
        
            // 否则将其添加到本地队列中
            t_schedq.push(worker);
        }

        // 查看本地 thread local 
        if (t_schedq.empty()){
            continue;
        }

        // 调度一次协程队列
        sync::Coroutine::ptr worker = t_schedq.front();
        t_schedq.pop();
        worker->go();
        // 任务执行完成，进入下一个循环
        if (worker->getState() == sync::Coroutine::Dead){
            continue;
        } 
        
        // 否则将其添加回到本地队列中
        t_schedq.push(worker);
    }
}

// 提交任务
// cb 被组装成一个 item，随机分配给到某个线程的本地队列中
bool WorkerPool::submit(task task){
    if (this->m_closed){
        return false;
    }

    localq taskq = this->getLocalQueueByThreadName(this->getThreadNameByIndex((s_taskId++)%this->m_threadPool.size()));
    taskq->push(task);
    return true;
}

std::string WorkerPool::getThreadNameByIndex(int index){
    return "workerPool_thread_" + std::to_string(index);
}

WorkerPool::localq WorkerPool::getLocalQueue(){
    sync::Thread::ptr curThread = sync::Thread::GetThis();
    if (!curThread){   
        throw std::exception();
    }

    std::string threadName = curThread->getName();
    return this->getLocalQueueByThreadName(threadName);
}

WorkerPool::localq WorkerPool::getLocalQueueByThreadName(std::string threadName){
    auto item = this->m_taskQueues.find(threadName);
    if (item == this->m_taskQueues.end()){
        throw std::exception();
    }

    return item->second;
}

}}