#pragma once 

#include <memory>
#include <functional>

#include "../sync/coroutine.h"
#include "../sync/channel.h"
#include "../sync/thread.h"
#include "../base/nocopy.h"

namespace cbricks{namespace pool{

class CoroutinePool : base::Noncopyable{
public:
    typedef std::shared_ptr<CoroutinePool> ptr;

public:
    // 构造/析构函数
    // threads——使用的线程个数
    CoroutinePool(size_t threads = 4);
    ~CoroutinePool();

public:
    // 提交任务
    // cb 被组装成一个 item，投递到 channel 中
    // item 被一个线程取到之后，就属于一个线程了
    bool submit(std::function<void()> cb);

private:
    // 协程池调度函数
    void schedule();

private:
    // 阻塞队列
    sync::Channel<std::function<void()>>::ptr m_channel;

    // 线程池
    std::vector<sync::Thread::ptr> m_threadPool;

    // 池子是否已关闭
    std::atomic<bool> m_closed{false};
};

}}