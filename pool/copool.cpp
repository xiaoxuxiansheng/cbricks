#include <exception>
#include <queue>

#include "copool.h"

namespace cbricks{ namespace pool{

// 任务 id 计数器
static std::atomic<int> s_taskId{0};

// 线程私有的协程 list
static thread_local std::queue<WorkerPool::workerPtr> t_schedq;

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
        this->m_taskQueues.insert({threadName,localqPtr(new localq)});
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
    localqPtr taskq = this->getLocalQueue();

    while (true){
        // 防止饥饿，最多调度 10 次的 localq 的情况下，需要看一次 t_schedq
        // 本地队列有任务的情况下，优先处理本地队列
        bool taskqEmpty = false;
        for (int i = 0; i < 10; i++){
            if (!this->readAndGo(taskq,false)){
                taskqEmpty = true;
            }
        }

        // 获取协程私有的协程队列进行调度
        if (!t_schedq.empty()){
            // 调度一次协程队列
            workerPtr worker = t_schedq.front();
            t_schedq.pop();

            this->goWorker(worker);
            continue;         
        }

        // 本地队列仍有任务
        if (!taskqEmpty){
            continue;
        }

        // 本地队列和协程都是空的，则尝试从其他线程任务队列中窃取一半任务添加到本地队列
        

        // 基于阻塞模式读取一次任务
        this->readAndGo(taskq,true);
    }
}

bool WorkerPool::readAndGo(cbricks::pool::WorkerPool::localqPtr taskq,bool nonblock){
    task cb;
    // 非阻塞模式获取任务
    if (!taskq->read(cb,nonblock)){
        return false;
    }

    this->goTask(cb);    
    return true;
}

void WorkerPool::goTask(task cb){
    workerPtr _worker(new worker(cb));
    this->goWorker(_worker);
}

void WorkerPool::goWorker(workerPtr worker){
    worker->go();

    // 任务未执行完成，则添加到 threadlocal 中的协程队列中
    if (worker->getState() != sync::Coroutine::Dead){
        t_schedq.push(worker);
    }     
}


// 提交任务
// cb 被组装成一个 item，随机分配给到某个线程的本地队列中
bool WorkerPool::submit(task task){
    if (this->m_closed){
        return false;
    }

    localqPtr taskq = this->getLocalQueueByThreadName(this->getThreadNameByIndex((s_taskId++)%this->m_threadPool.size()));
    taskq->write(task);
    return true;
}

std::string WorkerPool::getThreadNameByIndex(int index){
    return "workerPool_thread_" + std::to_string(index);
}

WorkerPool::localqPtr WorkerPool::getLocalQueue(){
    sync::Thread::ptr curThread = sync::Thread::GetThis();
    if (!curThread){   
        throw std::exception();
    }

    std::string threadName = curThread->getName();
    return this->getLocalQueueByThreadName(threadName);
}

WorkerPool::localqPtr WorkerPool::getLocalQueueByThreadName(std::string threadName){
    auto item = this->m_taskQueues.find(threadName);
    if (item == this->m_taskQueues.end()){
        throw std::exception();
    }

    return item->second;
}

}}