#include <thread>

#include "instancepool.h"
#include "../trace/assert.h"

namespace cbricks{namespace pool{

/**
 * @brief：构造函数
 * @param：_constructF——实例构造函数
 * @param：level——资源粒度分级，默认分成 8 份
 * @param：expDuration——过期回收时间，表示 instance 在对象池中闲置多长时间后会被自动回收
*/
InstancePool::InstancePool(constructF _constructF, const int level, const ms expDuration):m_constructF(_constructF),m_levelSize(level){
    CBRICKS_ASSERT(_constructF != nullptr, "constructor is empty");
    CBRICKS_ASSERT(level > 0, "level is nonpositive");
    CBRICKS_ASSERT(expDuration.count(), "expDuration is nonpositive");

    // evict thread 轮询间隔为用户设定 instance 过期时长 expDuration 的一半
    int64_t ecivtInterval = expDuration.count() / 2;
    if (ecivtInterval <= 0){
        ecivtInterval = 1;
    }
    this->m_evictInterval = ms(ecivtInterval);

    // 完成 local 中指定数量 levelPool 的初始化
    this->m_local.reserve(level);
    for (int i = 0; i < level; i++){
        this->m_local.push_back(LevelPool::ptr(new LevelPool));
    }

    // 异步启动 evict thread
    thread thr(std::bind(&InstancePool::asyncEvict,this));
}
    
// 析构函数
InstancePool::~InstancePool(){
    // 将 m_closed 标识置为 true.  evict thread 感知到此信息后会自行退出
    this->m_closed.store(true);
    // 确保 evict thread 退出后再完成析构. m_sem.notify 方法在 evict thread 退出前执行
    this->m_sem.wait();
}

/**
 * @brief: 从 instance pool 中获取一个 instance 实例：
 *  - 0）根据 level 递增计数器，分配得到一个 level
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
Instance::ptr InstancePool::get(){
    // 获取 level 层级
    int level = (this->m_level++)%this->m_levelSize;
    // 从 m_local 中获取
    Instance::ptr got = this->getFromPool(this->m_local,level);
    if (!got){
        // 从 m_victim 中获取
        got = this->getFromPool(this->m_victim,level);
        if (!got){
            // 使用 constructF 兜底
            got = this->m_constructF();
        }
    }

    // 返回 instance 实例前进行 reset 重置
    got->clear();
    return got;
}

/**
 * @brief：将一个 instance 归还回到 instancePool
 * @param: instance——归还的 instance
 * - 1）根据 level 递增计数器，分配得到一个 level
 * - 2）根据 level 从 m_local 获取对应的 levelPool
 * - 3）尝试放置到当前 levelPool 的 private 中（ 加 level 粒度 spinLock，只有同 level 下的 get 和 put 行为会发生竞争 ）
 * - 4）放置到当前 levelPool 的 shared 队列（加 level 粒度 lock，可能和任意 level 的 get 和 put 行为发生竞争）
 */
void InstancePool::put(Instance::ptr instance){
    // 获取 level 层级对应的 levelPool
    LevelPool::ptr levelPool = this->m_local[(this->m_level++)%this->m_levelSize];
    {
        // 加自旋锁并尝试放置到 levelPool->single
        spinLock::lockGuard guard(levelPool->singleLock);
        if (!levelPool->single){
            levelPool->single = instance;
            return;
        }
    }

    // 加互斥锁，并追加到 levelPool->shared 中
    lock::lockGuard guard(levelPool->sharedLock);
    levelPool->shared.push(instance);
}

/**
 * @brief: [异步执行] 定时清理达到过期时长的 instance
 * - 1）每隔 expDuration/2 执行一次
 * - 2）构造新的 m_local 实例
 * - 3）回收 m_victim
 * - 4）将前一轮的 m_local 变为 m_victim
 * - 5）将新的 m_local 变为 m_local
 */
void InstancePool::asyncEvict(){
    // thread 退出前必须执行一次 sem.notify.
    defer d([this](){
        this->m_sem.notify();
    });

    // 在 instance pool 未关闭前持续运行
    while (!this->m_closed.load()){
        // 每间隔 expDuration/2 执行一次
        std::this_thread::sleep_for(this->m_evictInterval);

        // 构造新的 local
        std::vector<LevelPool::ptr> newLocal;
        newLocal.reserve(this->m_local.size());
        for (int i = 0; i < this->m_local.size();i++){
            newLocal.push_back(LevelPool::ptr(new LevelPool));
        }

        // 新老 m_local/m_victim 轮换
        this->m_victim = this->m_local;
        this->m_local = newLocal;
    }
}

/**
 * @brief: 从某个指定 levelPool 中获取 instance 实例
 * @param：levelPool——指定的 levelPool
 * @param：获取 instance 过程中，是否忽略 levelPool 中的 single instance 实例. 默认为 false
 */
Instance::ptr InstancePool::getFromLevelPool(LevelPool::ptr levelPool, bool ignoreSingle){
    // 尝试从当前 levelPool 的 single 中获取 instance 实例
    if (!ignoreSingle){
        spinLock::lockGuard guard(levelPool->singleLock);
        if (levelPool->single){
            Instance::ptr got = levelPool->single;
            levelPool->single = nullptr;
            return got;
        }
    }

    // 尝试从当前 levelPool 的 shared 中获取 instance 实例
    lock::lockGuard guard(levelPool->sharedLock);
    if (!levelPool->shared.empty()){
        Instance::ptr got = levelPool->shared.front();
        levelPool->shared.pop();
        return got;
    }   

    return nullptr;
}

/**
 * @brief: 从某个 pool 的指定 level 中获取 instance 实例
 * @param：pool——m_local 或者 m_victim
 * @param：level——指定 level 层级
 */
Instance::ptr InstancePool::getFromPool(std::vector<LevelPool::ptr>& pool, int level){
    if (pool.empty()){
        return nullptr;
    }

    // 从当前 level 的 levelPool 中获取. 会分别尝试 single 和 shared
    Instance::ptr got = this->getFromLevelPool(pool[level]);
    if (got){
        return got;
    }

    // 从其他 level 的 levelPool 中获取. 只尝试 shared.
    for (int i = 0; i < pool.size(); i++){
        if (i == level){
            continue;
        }

        got = this->getFromLevelPool(pool[i],true);
        if (got){
            return got;
        }
    }

    return nullptr;    
}


}}