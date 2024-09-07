// 标准库队列实现. 依赖队列作为线程本地协程队列的存储载体
#include <queue>

// workerpool 头文件
#include "workerpool.h"
// 本项目定义的断言头文件
#include "../trace/assert.h"

// namespace cbricks::pool
namespace cbricks{ namespace pool{

/**
 * 全局变量 s_taskId：用于分配任务 id 的全局递增计数器，通过原子变量保证并发安全
 * 每个任务函数会根据分配到的 id，被均匀地分发给各个线程，以此实现负载均衡
*/
static std::atomic<int> s_taskId{0};

/**
 * 线程本地变量  t_schedq：线程私有的协程队列
 * 当线程下某个协程没有一次性将任务执行完成时（任务调用了 sched 让渡函数），则该协程会被暂存于此队列中，等待后续被相同的线程继续调度
 */
static thread_local std::queue<WorkerPool::workerPtr> t_schedq;

/**
 * workerpool 构造函数：
 * - 初始化好各个线程实例 thread
 * - 将各 thread 添加到线程池 m_threadPool 中
 */
WorkerPool::WorkerPool(size_t threads){
    CBRICKS_ASSERT(threads > 0, "worker pool init with nonpositive threads num");

    // 为线程池预留好对应的容量
    this->m_threadPool.reserve(threads);

    /**
     * 构造好对应于每个 thread 的信号量
     * 这是为了保证 thread 实例先被添加进入 m_threadPool，thread 中的调度函数才能往下执行
     * 这样做是因为 thread 调度函数有依赖于从 m_threadPool 获取自身实例的操作
    */
    std::vector<semaphore> sems(threads);
    // 另一个信号量，用于保证所有 thread 调度函数都正常启动后，当前构造函数才能退出，避免 sems 被提前析构
    semaphore waitGroup;

    // 初始化好对应数量的 thread 实例并添加进入 m_threadPool
    for (int i = 0; i < threads; i++){
        // 根据 index 映射得到 thread 名称
        std::string threadName = WorkerPool::getThreadNameByIndex(i);
        // 将 thread 实例添加进入 m_threadPool
        this->m_threadPool.push_back(thread::ptr(
            // thread 实例初始化
            new thread(
            i,
            // 
            new sync::Thread([this,&sems,&waitGroup](){
                /**
                 * 此处 wait 操作是需要等待对应 thread 实例先被推送进入 m_threadPool
                 * 因为一旦后续的 work 函数运行，就会涉及从 m_threadPool 中获取 thread 实例的操作
                 * 因此先后顺序不能颠倒
                 */
                sems[getThreadIndex()].wait();
                /**
                 * 此处 notify 操作是配合外层的 waitGroup.wait 操作
                 * 保证所有 thread 都正常启动后，workerPool 构造函数才能退出
                 * 这是为了防止 sems 被提前析构
                 */
                waitGroup.notify();
                // 异步启动的 thread，最终运行的调度函数是 workerpool::work
                this->work();
                
            },
            // 注入 thread 名称，与 index 有映射关系
            threadName),
            // 分配给 thread 的本地任务队列
            localqPtr(new localq))));
        /**
         * 在 thread 实例被推送入 m_threadPool 后进行 notify
         * 这样 thread 调度函数才会被向下放行
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
    // 将 workpool 的关闭标识置为 true，后续运行中的线程感知到此标识后会主动退出
    this->m_closed.store(true);
    // 等待所有线程都退出后，再退出 workpool 的析构函数
    for (int i = 0; i < this->m_threadPool.size(); i++){
        // 关闭各 thread 的本地任务队列
        this->m_threadPool[i]->taskq->close();
        // 等待各 thread 退出
        this->m_threadPool[i]->thr->join();
    }
}

/**
 * submit: 提交一个任务到协程调度池中，任务以闭包函数 void() 的形式组装
 * - 为任务分配全局递增且唯一的 taskId
 * - 根据 taskId 将任务均匀分发给指定 thread
 * - 将任务写入到指定 thread 的本地任务队列中
 */
bool WorkerPool::submit(task task, bool nonblock){
    // 若 workerpool 已关闭，则提交失败
    if (this->m_closed.load()){
        return false;
    }

    // 基于任务 id 对 m_threadPool 长度取模，将任务映射到指定 thread
    int targetThreadId = (s_taskId++)%(this->m_threadPool.size());
    thread::ptr targetThr = this->m_threadPool[targetThreadId];

    // 针对目标 thread 加读锁，这是为了防止和目标 thread 的 workstealing 操作并发最终因任务队列 taskq 容量溢出而导致死锁
    rwlock::readLockGuard guard(targetThr->lock);

    // 往对应 thread 的本地任务队列中写入任务
    return targetThr->taskq->write(task, nonblock);
}

// sched：让渡函数. 在任务执行过程中，可以通过该方法主动让出线程的执行权，则此时任务所属的协程会被添加到 thread 的本地协程队列 t_schedq 中，等待后续再被调度执行
void WorkerPool::sched(){
    worker::GetThis()->sched();
}

/**
 * work: 线程运行的主函数
 * 1） 获取需要调度的协程（下述任意步骤执行成功，则跳到步骤 2））
 *   - 从本地任务队列 taskq 中取任务，获取成功则为之初始化协程实例
 *   - 从本地协程队列 schedq 中取协程
 *   - 从其他线程的任务队列 taskq 中偷取一半任务到本地任务队列
 * 2） 调度协程执行任务
 * 3) 针对主动让渡而退出的协程，添加到本地协程队列
 * 4) 循环上述流程
 */
void WorkerPool::work(){
    // 获取到当前 thread 对应的本地任务队列 taskq
    localqPtr taskq = this->getThread()->taskq;

    // main loop
    while (true){
        // 如果 workerpool 已关闭 则主动退出
        if (this->m_closed.load()){
            return;
        }

        /**  
         * 执行优先级为 本地任务队列 taskq -> 本地协程队列 t_t_schedq -> 窃取其他线程任务队列 other_taskq
         * 为防止饥饿，至多调度 10 次的 taskq 后，必须尝试处理一次 t_schedq 
        */

        // 标识本地任务队列 taskq 是否为空
        bool taskqEmpty = false;
        // 至多调度 10 次本地任务队列 taskq
        for (int i = 0; i < 10; i++){
            // 以【非阻塞模式】从 taskq 获取任务并为之分配协程实例和调度执行
            if (!this->readAndGo(taskq,false)){
                // 如果 taskq 为空，将 taskqEmpty 置为 true 并直接退出循环
                taskqEmpty = true;
                break;
            }
        }

        // 尝试从线程本地的协程队列 t_schedq 中获取协程并进行调度
        if (!t_schedq.empty()){
            // 从协程队列中取出头部的协程实例
            workerPtr worker = t_schedq.front();
            t_schedq.pop();
            // 进行协程调度
            this->goWorker(worker);
            // 处理完成后直接进入下一轮循环
            continue;         
        }

        // 如果未发现 taskq 为空，则无需 workstealing，直接进入下一轮循环
        if (!taskqEmpty){
            continue;
        }

        /** 
         * 走到这里意味着 taskq 和 schedq 都是空的，则要尝试发起窃取操作
         * 随机选择一个目标线程窃取半数任务添加到本地队列中
        */
        this->workStealing();

        /**  
         * 以【阻塞模式】尝试从本地任务获取任务并调度执行
         * 若此时仍没有可调度的任务，则当前 thread 陷入阻塞，让出 cpu 执行权
         * 直到有新任务分配给当前 thread 时，thread 才会被唤醒
        */
        this->readAndGo(taskq,true);
    }
}

/**
 * readAndGo：
 *   - 从指定任务队列中获取一个任务
 *   - 为之分配协程实例并调度执行
 *   - 若协程实例未一次性执行完成（执行了让渡 sched），则将协程添加到线程本地的协程队列 schedq 中
 * param：taskq——任务队列；nonblock——是否以非阻塞模式从任务队列中获取任务
 * response：true——成功；false，失败（任务队列 taskq 为空）
 */
// 将一个任务包装成协程并进行调度. 如果没有一次性调度完成，则将协程实例添加到线程本地的协程队列 t_schedq
bool WorkerPool::readAndGo(cbricks::pool::WorkerPool::localqPtr taskq, bool nonblock){
    // 任务容器
    task cb;
    // 从 taskq 中获取任务
    if (!taskq->read(cb,nonblock)){
        return false;
    }

    // 对任务进行调度
    this->goTask(cb);    
    return true;
}

/**
 * goTask
 *   - 为指定任务分配协程实例
 *   - 执行协程
 *   - 若协程实例未一次性执行完成（执行了让渡 sched），则将协程添加到线程本地的协程队列 schedq 中
 * param：cb——待执行的任务
 */
void WorkerPool::goTask(task cb){
    // 初始化协程实例
    workerPtr _worker(new worker(cb));
    // 调度协程
    this->goWorker(_worker);
}

/**
 * goWorker
 *   - 执行协程
 *   - 若协程实例未一次性执行完成（执行了让渡 sched），则将协程添加到线程本地的协程队列 schedq 中
 * param：worker——待运行的协程
 */
void WorkerPool::goWorker(workerPtr worker){
    // 调度协程，此时线程的执行权会切换进入到协程对应的方法栈中
    worker->go();
    // 走到此处意味着线程执行权已经从协程切换回来
    // 如果此时协程并非已完成的状态，则需要将其添加到线程本地的协程队列 schedq 中，等待后续继续调度
    if (worker->getState() != sync::Coroutine::Dead){
        t_schedq.push(worker);
    }     
}

// 从某个 thread 中窃取一半任务给到本 thread 的 taskq
void WorkerPool::workStealing(){   
    // 选择一个窃取的目标 thread 
    thread::ptr stealFrom = this->getStealingTarget();
    if (!stealFrom){
        return;
    }
    // 从目标 thread 中窃取半数任务添加到本 thread taskq 中
    this->workStealing(this->getThread(),stealFrom);
}

// 从 thread:stealFrom 中窃取半数任务给到 thread:stealTo
void WorkerPool::workStealing(thread::ptr stealTo, thread::ptr stealFrom){    
    // 确定窃取任务数量：目标本地任务队列 taskq 中任务总数的一半
    int stealNum = stealFrom->taskq->size() / 2;
    if (stealNum <= 0){
        return;
    }

    // 针对 thread:stealTo 加写锁，防止因 workstealing 和 submit 行为并发，导致线程因 taskq 容量溢出而发生死锁
    rwlock::lockGuard guard(stealTo->lock);
    // 检查此时 stealTo 中的 taskq 如果剩余容量已不足以承载拟窃取的任务量，则直接退出
    if (stealTo->taskq->size() + stealNum > stealTo->taskq->cap()){
        return;
    }

    // 创建任务容器，以非阻塞模式从 stealFrom 的 taskq 中窃取指定数量的任务
    std::vector<task> containers(stealNum);
    if (!stealFrom->taskq->readN(containers,true)){
        return;
    }

    // 将窃取到的任务添加到 stealTo 的 taskq 中
    stealTo->taskq->writeN(containers,false);
}

// 随机选择本 thread 外的一个 thread 作为窃取的目标
WorkerPool::thread::ptr WorkerPool::getStealingTarget(){
    // 如果线程池长度不足 2，直接返回
    if (this->m_threadPool.size()< 2){
        return nullptr;
    }

    // 通过随机数，获取本 thread 之外的一个目标 thread index
    int threadIndex = WorkerPool::getThreadIndex();
    int targetIndex = rand() % this->m_threadPool.size();
    while ( targetIndex == threadIndex){
        targetIndex = rand() % this->m_threadPool.size();
    }

    // 返回目标 thread
    return this->m_threadPool[targetIndex];
}

// 基于线程在线程池中 index 映射得到线程名称
const std::string WorkerPool::getThreadNameByIndex(int index){
    return "workerPool_thread_" + std::to_string(index);
}

// 获取当前线程的线程名
const std::string WorkerPool::getThreadName(){
    sync::Thread* curThread = sync::Thread::GetThis();
    CBRICKS_ASSERT(curThread != nullptr, "get current thread fail");

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