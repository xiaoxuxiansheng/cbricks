#pragma once 

// 时间相关
#include <chrono>
// 函数编程相关
#include <functional>
// 智能指针相关
#include <memory>
// 队列
#include <queue>
// 原子变量、原子操作
#include <atomic>

// 禁用类的拷贝、赋值操作
#include "../base/nocopy.h"
// 效仿 golang defer 机制. 利用栈内变量析构机制，保证栈回收前执行指定任务
#include "../base/defer.h"
// 锁相关，包含自旋锁 spinLock 互斥锁 lock
#include "../sync/lock.h"
// 线程相关
#include "../sync/thread.h"
// 信号量相关
#include "../sync/sem.h"

namespace cbricks{namespace pool{

/**
 * 对象实例的 interface 定义
 */
class Instance{
public:
    typedef std::shared_ptr<Instance> ptr;
public:
    Instance() = default;
    /**
     * @brief：析构函数 置为 virtual，保证继承类的析构函数能被正常调用
     */
    virtual ~Instance() = default;
    /**
     * @brief：重置实例中的数据——纯虚函数，需要被实现
     */
    virtual void clear() = 0;
};

/**
 * 对象池，不可值拷贝和值复制
 */
class InstancePool : base::Noncopyable{
public:
    // 实例构造函数 类型别名
    typedef std::function<Instance::ptr()> constructF;
    // 毫秒 类型别名
    typedef std::chrono::milliseconds ms;
    // 方法栈回收前的兜底执行函数
    typedef base::Defer defer;

    /** 并发相关 */
    // 线程 类型别名
    typedef sync::Thread thread; 
    // 信号量 类型别名
    typedef sync::Semaphore semaphore;
    // 自旋锁 类型别名
    typedef sync::SpinLock spinLock;
    // 互斥锁 类型别名
    typedef sync::Lock lock;
public:
    /**
     * @brief：构造函数
     * @param：_constructF——实例构造函数
     * @param：level——资源粒度分级，默认分成 8 份
     * @param：expDuration——过期回收时间，表示 instance 在对象池中闲置多长时间后会被自动回收
     */
    InstancePool(constructF _constructF, const int level = 8, const ms expDuration = ms(500));
    /** 析构函数 */
    ~InstancePool();

private:
    /**
     * 某个 level 下的 instance 队列
     */
    struct LevelPool{
        // 共享指针 类型别名
        typedef std::shared_ptr<LevelPool> ptr;
        // 保护私有 instance 实例的自旋锁
        spinLock singleLock;
        // 私有 instance 实例
        Instance::ptr single = nullptr;

        // 保护共享 instance 队列的互斥锁
        lock sharedLock;
        // 共享 instance 队列
        std::queue<Instance::ptr> shared;
    };

public:
    /**
     * @brief: 从 instance pool 中获取一个 instance 实例：
     * - 0）根据 level 递增计数器，分配得到一个 level
     * - 针对 m_local 操作：
     *  - 1）根据 level 获取对应的 levelPool
     *  - 2）尝试获取当前 levelPool 的 private instance（ 加 level 粒度 spinLock，只有同 level 下的 get 和 put 行为会发生竞争 ）
     *  - 3）尝试从当前 levelPool 的 shared 队列中获取 instance（加 level 粒度 lock，可能和任意 level 的 get 和 put 行为发生竞争）
     *  - 4） 尝试从其他 levelPool 的 shared 队列中获取 instance（加其他 level 粒度 lock，可能和任意 level 的 get 和 put 行为发生竞争）
     *  - 5）使用 newFc 构造出新实例
     * - 针对 m_victim 操作：
     *  - 6）根据 level 获取对应的 levelPool
     *  - 7）尝试获取当前 levelPool 的 private instance（ 加 level 粒度 spinLock，只有同 level 下的 get 和 put 行为会发生竞争 ）
     *  - 8）尝试从当前 levelPool 的 shared 队列中获取 instance（加 level 粒度 lock，可能和任意 level 的 get 和 put 行为发生竞争）
     *  - 9） 尝试从其他 levelPool 的 shared 队列中获取 instance（加其他 level 粒度 lock，可能和任意 level 的 get 和 put 行为发生竞争）
     *  - 10）使用 newFc 构造出新实例
     * @return: 返回获取到的实例
     */
    Instance::ptr get();

    /**
     * @brief：将一个 instance 归还回到 instancePool
     * @param: instance——归还的 instance
     * - 1）根据 level 递增计数器，分配得到一个 level
     * - 2）根据 level 从 m_local 获取对应的 levelPool
     * - 3）尝试放置到当前 levelPool 的 private 中（ 加 level 粒度 spinLock，只有同 level 下的 get 和 put 行为会发生竞争 ）
     * - 4）放置到当前 levelPool 的 shared 队列（加 level 粒度 lock，可能和任意 level 的 get 和 put 行为发生竞争）
     */
    void put(Instance::ptr instance); 

private:
    /**
     * @brief: [异步执行] 定时清理达到过期时长的 instance
     * - 1）每隔 expDuration/2 执行一次
     * - 2）构造新的 m_local 实例
     * - 3）回收 m_victim
     * - 4）将前一轮的 m_local 变为 m_victim
     * - 5）将新的 m_local 变为 m_local
     */
    void asyncEvict();

    /**
     * @brief: 从某个指定 levelPool 中获取 instance 实例
     * @param：levelPool——指定的 levelPool
     * @param：获取 instance 过程中，是否忽略 levelPool 中的 single instance 实例. 默认为 false
     */
    Instance::ptr getFromLevelPool(LevelPool::ptr levelPool, const bool ignoreSingle = false);
    
    /**
     * @brief: 从某个 pool 的指定 level 中获取 instance 实例
     * @param：pool——m_local 或者 m_victim
     * @param：level——指定 level 层级
     */
    Instance::ptr getFromPool(std::vector<LevelPool::ptr>& pool, int level);

private:
    // evict thread 轮询间隔
    ms m_evictInterval;
    // 用于构造 instance 实例的构造器函数
    constructF m_constructF;
    // 当前轮次的 instance 缓冲池
    std::vector<LevelPool::ptr> m_local;
    // 上一轮次的 instance 缓冲池
    std::vector<LevelPool::ptr> m_victim;
    // level 计数器
    std::atomic<int> m_level{0};
    int m_levelSize;

    // 标识 instance pool 是否关闭
    std::atomic<bool> m_closed{false};
    // 信号量，用于在 instance pool 析构时控制保证 evict thread 先行退出
    semaphore m_sem;
};


}}