#include <exception>
#include <queue>
#include <cstdlib>

#include "workerpool.h"

namespace cbricks{ namespace pool{

// 用于分配任务 id 的计数器. 任务根据自己的 id 会被均匀分配给各个线程
static std::atomic<int> s_taskId{0};

// 一个线程私有的协程队列，当协程中一个任务没有一次性执行完成时，则会被暂存到此队列中等待后续被相同的线程调度
static thread_local std::queue<WorkerPool::workerPtr> t_schedq;

// 构造函数
WorkerPool::WorkerPool(size_t threads){
    if (threads <= 0){
        throw std::exception();
    }

    // 初始化好线程池中的每个线程实例
    this->m_threadPool.reserve(threads);

    std::vector<semaphore> sems(threads);
    semaphore waitGroup;
    for (int i = 0; i < threads; i++){
        // 根据 index 映射得到线程名称
        std::string threadName = WorkerPool::getThreadNameByIndex(i);
        // 启动线程实例，该线程异步执行的方法为 WorkerPool::work
        // 将线程实例添加到线程池中
        // 通过信号量 保证线程实例先进入池子，然后再运行线程函数
        this->m_threadPool.push_back(thread::ptr(
            new thread(
            i,
            new sync::Thread([this,&sems,&waitGroup](){
                /**
                 * 此处 wait 操作是为了等待对应 thread 实例被推送进入 thread pool
                 * 因为一旦 work 函数运行，就会需要从 thread pool 中获取 thread 实例
                 */
                sems[getThreadIndex()].wait();
                /**
                 * 此处 notify 操作是配合外层的 waitGroup.wait 操作
                 * 保证所有 thread 都正常启动后，workerPool 构造函数才能退出
                 * 这是为了防止 sems 被提前析构
                 */
                waitGroup.notify();
                this->work();
            },threadName),
            localqPtr(new localq))));
        /**
         * 在 thread 实例被推送入 pool 后进行 notify
         * 放行对应的线程执行函数
         */
        sems[i].notify();
    }

    /**
     * 等待所有 thread 实例正常启动后，构造函数再退出
     */
    for (int i = 0; i < threads; i++){
        waitGroup.wait();
    }
}

// 析构函数
WorkerPool::~WorkerPool(){
    // 将关闭标识置为 true，后续线程感知到此情况后会主动退出
    this->m_closed = true;
    // 等待所有线程都退出后完成析构
    for (int i = 0; i < this->m_threadPool.size(); i++){
        this->m_threadPool[i]->taskq->close();
        // this->m_threadPool[i]->thr->join();
    }
}

// 提交一个任务到协程调度池中，任务以闭包函数形式组装，根据任务 id 均匀分配给各个线程
bool WorkerPool::submit(task task, bool nonblock){
    // 池子若已关闭，则不再提交
    if (this->m_closed){
        return false;
    }

    // 基于任务 id 对线程池长度取模，获取任务所属线程 index
    int targetThreadId = (s_taskId++)%(this->m_threadPool.size());

    // 此任务分配给到的目标线程
    thread::ptr targetThr = this->m_threadPool[targetThreadId];

    // 针对目标线程加读锁，防止和目标线程的窃取操作产生并发冲突导致死锁
    rwlock::readLockGuard guard(targetThr->lock);

    // 往对应线程的队列中写入任务
    return targetThr->taskq->write(task, nonblock);
}

// 在闭包函数执行过程中，可以通过该方法主动让出线程的执行权
void WorkerPool::sched(){
    worker::GetThis()->sched();
}

// 一个线程持续运行的调度主流程
void WorkerPool::work(){
    // 获取到当前线程对应的本地任务队列 
    localqPtr taskq = this->getThread()->taskq;

    while (true){
        // 如果协程调度池关闭了，则无视后续任务直接退出
        if (this->m_closed){
            return;
        }

        /**  
         * 调度优先级为 本地任务队列 taskq -> 
         * 本地协程队列 t_t_schedq -> 
         * 窃取其他线程的本地任务队列 other_taskq
         * 为防止饥饿，每调度 10 次的 localq 后，必须尝试处理一次 t_schedq 
        */

        // 标识本地任务队列是否为空
        bool taskqEmpty = false;

        // 尝试进行 10 次本地任务队列调度
        for (int i = 0; i < 10; i++){
            // 针对本地队列任务调度，如果为空，则将标识置为 true 并且 break
            if (!this->readAndGo(taskq,false)){
                taskqEmpty = true;
                break;
            }
        }

        // 调度一次线程私有的协程队列 t_schedq
        if (!t_schedq.empty()){
            // 从协程队列中取出头部的协程
            workerPtr worker = t_schedq.front();
            t_schedq.pop();
            // 进行协程调度
            this->goWorker(worker);
            continue;         
        }

        // 如果本地队列仍有任务，则进入下一个循环
        if (!taskqEmpty){
            continue;
        }

        /** 
         * 走到这里意味着本地任务队列和协程队列都是空的
         * 此时需要随机选择一个目标线程窃取半数任务添加到本地队列
        */
        this->workStealing();

        // 以阻塞模式获取本地任务，如果陷入阻塞则说明没有任务需要处理
        this->readAndGo(taskq,true);
    }
}

// 将一个任务包装成协程并进行调度. 如果没有一次性调度完成，则将协程实例添加到线程本地的协程队列 t_schedq
bool WorkerPool::readAndGo(cbricks::pool::WorkerPool::localqPtr taskq,bool nonblock){
    task cb;
    // 按照指定模式从本地队列中获取任务
    if (!taskq->read(cb,nonblock)){
        return false;
    }

    // 对任务进行调度
    this->goTask(cb);    
    return true;
}

// 为任务创建协程实例并进行调度
void WorkerPool::goTask(task cb){
    // 封装成协程实例
    workerPtr _worker(new worker(cb));
    // 调度协程
    this->goWorker(_worker);
}

// 调度协程
void WorkerPool::goWorker(workerPtr worker){
    // 让出线程 main 协程执行权给到工作协程执行
    worker->go();

    // 从工作协程切回 main，如果此时协程状态不是已完成，则将其添加到线程本地的协程队列 t_schedq 中
    if (worker->getState() != sync::Coroutine::Dead){
        t_schedq.push(worker);
    }     
}

// 随机从本线程之外的某个线程中窃取一半的任务给到本线程
void WorkerPool::workStealing(){    
    thread::ptr stealFrom = this->getStealingTarget();
    if (!stealFrom){
        return;
    }
    this->workStealing(this->getThread(),stealFrom);
}

// 从 线程：stealFrom 中窃取一半的任务给到线程：stealTo
void WorkerPool::workStealing(thread::ptr stealTo, thread::ptr stealFrom){    
    // 窃取目标任务队列任务总数的一半
    int stealNum = stealFrom->taskq->size() / 2;
    // 如果可窃取任务数为 0，则直接返回
    if (stealNum <= 0){
        return;
    }

    // 针对拟塞入任务的线程加写锁，使得窃取行为和任务提交行为发生隔离，防止死锁
    rwlock::lockGuard guard(stealTo->lock);

    // 如果此时目标任务队列容量不够，则不作窃取直接返回
    if (stealTo->taskq->size() + stealNum > stealTo->taskq->cap()){
        return;
    }

    // 创建承载窃取任务的容器
    std::vector<task> containers(stealNum);
    // 以非阻塞模式窃取指定数量的任务
    if (!stealFrom->taskq->readN(containers,true)){
        return;
    }

    // 以阻塞模式，将窃取到的任务添加到目标队列中
    stealTo->taskq->writeN(containers);
}

// 随机获取本线程之外的一个线程作为拟窃取的目标线程
WorkerPool::thread::ptr WorkerPool::getStealingTarget(){
    // 如果线程池长度不足，直接返回空
    if (this->m_threadPool.size()< 2){
        return nullptr;
    }

    // 获取本线程的 index
    int threadIndex = WorkerPool::getThreadIndex();
    // 通过随机数获取拟窃取目标线程 index，要求其不等于当前线程 index
    int targetIndex = rand() % this->m_threadPool.size();
    while ( targetIndex == threadIndex){
        targetIndex = rand() % this->m_threadPool.size();
    }

    return this->m_threadPool[targetIndex];
}

// 基于线程在线程池中 index 映射得到线程名称
const std::string WorkerPool::getThreadNameByIndex(int index){
    return "workerPool_thread_" + std::to_string(index);
}

// 获取当前线程的线程名
const std::string WorkerPool::getThreadName(){
    sync::Thread* curThread = sync::Thread::GetThis();
    if (!curThread){   
        throw std::exception();
    }

    return curThread->getName();
}

// 获取当前线程在线程池中的 index
const int WorkerPool::getThreadIndex(){
    std::string threadName = WorkerPool::getThreadName();
    return std::stoi(threadName.erase(0, std::string("workerPool_thread_").size()));
}

// 获取当前线程实例
WorkerPool::thread::ptr WorkerPool::getThread(){
    return this->m_threadPool[WorkerPool::getThreadIndex()];
}

}}