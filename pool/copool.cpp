#include <exception>
#include <queue>

#include "copool.h"

namespace cbricks{ namespace pool{

// 线程私有的协程 list
static thread_local std::queue<sync::Coroutine::ptr> t_localPool;

CoroutinePool::CoroutinePool(size_t threads){
    if (threads <= 0){
        throw std::exception();
    }

    // 初始化好线程池中的每个线程
    this->m_threadPool.reserve(threads);
    for (int i = 0; i < this->m_threadPool.size();i++){
        sync::Thread::ptr worker(new sync::Thread(std::bind(&CoroutinePool::schedule,this)));
        this->m_threadPool.push_back(worker);
    }

    // 初始化好 channel. 队列容量设为线程数量的 8 倍
    this->m_channel.reset(new sync::Channel<std::function<void()>>(threads * 8));
}

CoroutinePool::~CoroutinePool(){
    this->m_closed = true;
    // 等待剩余任务都执行完成
    // 等待所有线程都关闭
    for (int i = 0; i < this->m_threadPool.size(); i++){
        this->m_threadPool[i]->join();
    }
}

void CoroutinePool::schedule(){
    while (true){
        // 本地队列有任务的情况下，优先处理本地队列
        while (!t_localPool.empty()){
            std::shared_ptr<sync::Coroutine> polled = t_localPool.front();
            t_localPool.pop();

            // 执行协程
            polled->go();
            // 回来时查看协程的状态，如果已运行终止，则任务完成，否则重新回到队列
            if (polled->getState() == sync::Coroutine::Dead){
                continue;
            }

            t_localPool.push(polled);
        }

        // 此时如果 pool 已经被关闭且全局队列为空，则可以退出
        if (this->m_closed && this->m_channel->empty()){
            return;
        }

        // 本地队列为空的情况下，取全局队列
        std::function<void()> cb;

        // channel 被关闭了，则判断是否
        this->m_channel->read(cb);

        // 执行任务函数为空，进入下一个循环
        if (!cb){
            continue;
        }
        
        sync::Coroutine::ptr worker(new sync::Coroutine(cb));
        worker->go();

        // 任务执行完成，进入下一个循环
        if (worker->getState() == sync::Coroutine::Dead){
            continue;
        } 
        
        // 否则将其添加到本地队列中
        t_localPool.push(worker);
    }
}

// 提交任务
// cb 被组装成一个 item，投递到 channel 中
// item 被一个线程取到之后，就属于一个线程了
bool CoroutinePool::submit(std::function<void()> cb){
    if (this->m_closed){
        return false;
    }
    return this->m_channel->write(cb);
}

}}