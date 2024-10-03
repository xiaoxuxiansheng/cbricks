#pragma once 

#include <atomic>
#include <memory>
#include <unordered_map>
#include <functional>

#include "../base/nocopy.h"
#include "lock.h"

namespace cbricks{namespace sync{

/**
 * @brief: 并发安全的 map 工具——借鉴 golang sync.Map 实现
 * 适用场景：适用于 插入类写操作较少的 场景
 * 功能点：底层基于以空间换时间的思路，建立 read dirty 两份 map
 *        - read：应用于读、更新、删除场景，实现无锁化
 *        - dirty：包含全量数据，访问时需要加锁
 */
template <class Key, class Value>
class Map : base::Noncopyable{
public:
    /**
     * 构造、析构函数
     */
    // 构造函数. 初始化 readonly 实例
    Map();
    // 析构函数. 回收 readonly 实例
    ~Map();

private:
    /**
     * map 中依赖的内置私有类型
     */
    // 底层存放 value 数据的容器
    struct Entry{
    public:
        // 智能指针 类型别名
        typedef std::shared_ptr<Entry> ptr;

    public:
        /**
         * 构造 析构函数
         */
        // 构造函数，入参为存储数据
        Entry(Value v);
        // 析构函数
        ~Entry();
    
    public:
        /**
         * entry 内置类
         */
        // 对存储数据使用该类型的指针进行封装
        struct WrappedV{
        public:
            WrappedV() = default;
            WrappedV(Value v);
        public:
            Value v;
        };

    public:
        /**
         * 读操作相关方法
         */
        /**
         * @brief: 从 entry 中读取数据，存储到 receiver 中
         * @param: 承载读取数据的容器
         * @return: true——读取成功 false——读取失败
         */
        bool load(Value& receiver);

    public:
        /**
         * 写操作相关
         */
        /**
         * @brief: 将数据存储在 entry 的原子容器中 [调用此方法时一定处于持有 dirtyLock 状态]
         * @param: v——拟存储的数据
         */
        void storeLocked(Value v);

        /**
         * @brief: 尝试将数据存储在 entry 的原子容器中
         * @param: v——尝试存储的数据
         * @return: true——存储成功 false——存储失败（硬删除态 expunged 时存储失败）
         */
        bool tryStore(Value v);

        // 删除 entry 中存储的数据，将 container 中内容置为 nullptr（软删除态）
        void evict();

    public:
        /**
         * entry 状态更新操作相关方法
         */
        /**
         * @brief: 将 entry 置为非硬删除态
         * @return: true——之前处于硬删除态（expunged），更新成功；false——之前处于非硬删除态，无需更新
         */
        bool unexpungeLocked();
        /**
         * @brief: 尝试将 entry 置为硬删除态（expunged）
         * @return: true——之前就是硬删除态，或者成功将 nil （软删除态）置为硬删除态； false——更新失败（此前数据未删除）
         */
        bool tryExpungeLocked();

    public:
        /** 
         * 存放数据的 atomic 容器 
         * WrappedV 分为三种可能：
         * 1）s_expunged：硬删除态，表示 dirty 中一定没有该 entry，要恢复数据必须对 dirty 执行 insert 操作
         * 2）nullptr：软删除态. 数据逻辑意义上已被删除，但是 dirty 中还有该数据，还能快速恢复过来
         * 3）其他：数据仍存在，未被删除
        */
        std::atomic<WrappedV*> container;
    };

public:
    // 智能指针 类型别名
    typedef std::shared_ptr<Map<Key,Value>> ptr;
    // 存储 key-entry 的 map 类型别名
    typedef std::unordered_map<Key, typename Entry::ptr> map;

private:
    /**
     * map 中依赖的内置私有类型
     * 由于内部仅有一个 bool 及共享指针字段，所以发生值拷贝的成本可忽略不计
     */
    // 只读 map 数据类型
    struct ReadOnly{
        // 构造函数
        ReadOnly();
        // 拷贝构造函数
        ReadOnly(const ReadOnly& other);
        // 赋值函数
        ReadOnly& operator=(const ReadOnly& other);
        /**
         * 标识 readonly 相比 dirty 是否存在数据缺失
         * 1）true——存在缺失，则操作 readonly 时若发现数据 miss，还需要读取 dirty 兜底
         * 2）false——不存在缺少. 则操作 readonly 时若发现数据 miss，无需额外操作 dirty
         */
        volatile bool amended;
        // 存储 readonly 中 kv 数据的 map. value 为 entry 共享指针类型，可供多个 readonly 实例共享
        std::shared_ptr<map> m;
    };

public:
    /**
     * 公有方法
     */
    /**
     * @brief：读取数据，存储到 receiver 中
     * @param: key——数据键
     * @param: receiver——接收数据的容器
     * @return: true——数据存在，false——数据不存在
     */
    bool load(Key key, Value& receiver);

    /**
     * @brief: 写入数据
     * @param: key——数据键
     * @param: value——拟写入的数据
     */
    void store(Key key, Value value);

    /**
     * @brief: 删除数据
     * @param: key——数据键
     */
    void evict(Key key);

    /**
     * @brief: 遍历 map
     * @param：f——用于接收 key-value 对的闭包函数
     *            key——数据键  value——数据
     */
    void range(std::function<bool(const Key& key, const Value& value)> f);

private:
    /**
     * @brief: 累计 readonly 数据 miss 次数
     * 当 miss 次数 >= dirty 大小时，需要执行 readonlyLocked 流程，使用 dirty 覆盖 readonly，并清空 dirty
     * 1) miss++  2) dirty -> readonly
     */
    void missLocked();
    
    /**
     * @brief：将 dirty 赋给 readonly，然后清空 dirty
     */
    void readonlyLocked();

    /**
     * @brief: 倘若 dirty 为空，需要遍历 readonly 将未删除的数据转移到 dirty 中
     * 遍历 readonly 时间复杂度 O(N)，同时过滤掉所有处于删除态（expunged or nullptr）的数据
     */
    void dirtyLocked();

private:
    /**
     * 使用全局静态变量标记特殊的硬删除状态 expunged
     * 当一个 key 于 dirty 中已不存在时，对应 entry 的 container 中会存储该实例，用于标记该项数据处于硬删除态（expunged）
    */ 
    static typename Map<Key,Value>::Entry::WrappedV* s_expunged;

private:
    /**
     * 私有方法
     */
    
    /**
     * 只读 map，全程无锁访问
     * 使用 atomic 容器承载，由于内部仅有一个 bool 及共享指针字段，所以发生值拷贝的成本可忽略不计
     */
    std::atomic<typename Map<Key,Value>::ReadOnly*> m_readonly;
    // 记录 readonly miss 次数，与 missLocked 流程紧密相关
    std::atomic<int> m_misses{0};

    // 互斥锁，访问 dirty map 前需要持有此锁
    Lock m_dirtyLock;
    // dirty map，包含全量数据
    map m_dirty;
};

// 全局静态变量，标记硬删除状态.
template <class Key, class Value>
typename Map<Key, Value>::Entry::WrappedV* Map<Key,Value>::s_expunged = new typename Map<Key,Value>::Entry::WrappedV();

// 构造函数，初始化 readonly 实例
template <class Key, class Value>
Map<Key,Value>::Map(){
    this->m_readonly.store(new typename Map<Key,Value>::ReadOnly);
}

// 析构函数，回收 readonly 实例
template <class Key, class Value>
Map<Key,Value>::~Map(){
    typename Map<Key,Value>::ReadOnly* readonly = this->m_readonly.load();
    delete readonly;
}

/**
 * @brief：读取数据
 * @param: key——数据键
 * @param: receiver——接收数据的容器
 * @return: true——数据存在，false——数据不存在
 */
template <class Key, class Value>
bool Map<Key,Value>::load(Key key, Value& receiver){
    /**
     * 核心处理流程：
     * 1）尝试从 readonly 中读取数据，若存在直接返回
     * 2）若 readonly.amended 为 false，代表 readonly 已有全量数据，也直接返回
     * 3）加锁
     * 4）再次从 readonly 中读取数据，若存在直接返回 (并发场景下的 double check)
     * 5）从 dirty 中读取数据，并且执行 missLocked 流程
     */

    // 尝试从 readonly 中读取数据，若存在直接返回
    typename Map<Key,Value>::Entry::ptr e = nullptr;

    // 从 atomic 容器中读取 readonly 实例指针，并执行拷贝构造函数，生成 readonly 实例
    typename Map<Key,Value>::ReadOnly readonly = *this->m_readonly.load();
    auto it = readonly.m->find(key);
    if (it != readonly.m->end()){
        e = it->second;
    }

    if (e != nullptr){
        return e->load(receiver);
    }
    
    // readonly.amended 为 false，表示 readonly 已包含全量数据，无需再读 dirty 了
    if (!readonly.amended){
        return false;
    }

    // readonly 中数据不存在，且 amended 为 true，则需要加锁读 dirty map
    // 加锁
    Lock::lockGuard guard(this->m_dirtyLock);

    // 加锁后对 readonly double check
    // 此处执行的是 readonly 赋值操作函数
    readonly = *this->m_readonly.load();
    it = readonly.m->find(key);
    if (it != readonly.m->end()){
        e = it->second;
    }

    if (e != nullptr){
        return e->load(receiver);
    }

    if (!readonly.amended){
        return false;
    }  

    // readonly 中数据不存在且 amended 为 true，读 dirty
    it = this->m_dirty.find(key);
    if (it != this->m_dirty.end()){
        e = it->second;
    }

    // 执行 missLocked 流程. 其中可能因为 miss 次数过多，而发生 dirty->readonly 的更新
    this->missLocked();
    
    // 若 readonly 和 dirty 中都不存在该 key，直接返回 false
    if (e == nullptr){
        return false;
    }

    // 尝试从 dirty 获取的 entry 中读取结果
    return e->load(receiver);
}

/**
 * @brief: 写入数据
 * @param: key——数据键
 * @param: value——拟写入的数据
 */
template <class Key, class Value>
void Map<Key,Value>::store(Key key, Value value){
    /**
     * 1）尝试从 readonly 中读取 key 对应 entry
     * 2）若 readonly 中存在该 entry，则尝试通过 cas 操作更新 entry 为新值，前提是 entry 不为硬删除态（expunged）
     * 3）若 2）cas 成功，直接返回
     * 4）加锁
     * 5）再次尝试从 readonly 中读取 key 对应 entry （并发场景下的 double check），若不存在，进入 8）
     * 6）确保将 entry 更新为非硬删除态，若此前为硬删除态（expunged），需要在 dirty 中补充该 entry
     * 7）向 entry 中存储新值，并返回
     * 8）尝试从 dirty 中读取 key 对应 entry，若存在，往 entry 中存储新值并返回
     * 9）此时注定要往 dirty 中插入新数据，若 readonly.amended 为 false，需要将其更新为 true
     * 10）若此时 dirty 为空，需要执行 dirtyLocked 流程，将 readonly 中所有非硬删除态的数据拷贝到 dirty 中
     * 11）往 dirty 中插入新的 entry 并返回
     */

    // 尝试从 readonly 中读取 key 对应 entry
    typename Map<Key,Value>::Entry::ptr e = nullptr;
    // 从 atomic 容器中读取 readonly 实例指针，并执行拷贝构造函数，生成 readonly 实例
    typename Map<Key,Value>::ReadOnly readonly = *this->m_readonly.load();
    auto it = readonly.m->find(key);
    if (it != readonly.m->end()){
        e = it->second;
    }

    // 尝试通过 cas 操作完成 value 的更新，前提是 entry 不为 expunged 状态
    if (e != nullptr && e->tryStore(value)){
        return;
    }

    // 加锁
    Lock::lockGuard guard(this->m_dirtyLock);

    // 加锁后对 readonly double check
    // 执行 readonly 赋值函数
    readonly = *this->m_readonly.load();
    it = readonly.m->find(key);
    if (it != readonly.m->end()){
        e = it->second;
    }

    // 若 entry 在 readonly 中存在，直接更新 value
    if (e != nullptr){
        // 尝试将 e 由硬删除态（expunuged）置为非硬删除态（nullptr）
        if (e->unexpungeLocked()){
            // 若返回 true 表示此前为硬删除态，需要在 dirty 中补充该数据
            this->m_dirty.insert({key,e});
        }
        // 将 entry 中的内容更新为 value 并返回
        e->storeLocked(value);
        return;
    }

    /**
     * 至此，已确定 readonly 中不存在数据
     */
    // 尝试从 dirty 中读取数据
    it = this->m_dirty.find(key);
    if (it != this->m_dirty.end()){
        e = it->second;
    }

    // 若 dirty 中存在该 entry, 直接更新即可（dirty 中存在 entry 时，不可能为硬删除态（expunged））
    if (e != nullptr){
        e->storeLocked(value);
        return;
    }

    /**
     * 至此已明确 readonly 和 dirty 中均不存在该 key，需要向 dirty 中插入新数据，此前需要执行一些准备工作
     * 1）若 readonly.amended 为 false，需要将其更新为 true
     * 2）若此前 dirty 为空，需要将 readonly 中所有非硬删除态的数据拷贝到 dirty（dirtyLocked 流程）
     */
    if (!readonly.amended){
        // atomic 容器中，readonly 实例的 amended 更新为 true
        this->m_readonly.load()->amended = true;
        this->dirtyLocked();
    }

    // 向 dirty 中插入新数据对应的 entry
    this->m_dirty.insert({key, typename Map<Key,Value>::Entry::ptr(new Entry(value))});
}

/**
 * @brief: 删除数据
 * @param: key——数据键
 */
template<class Key, class Value>
void Map<Key,Value>::evict(Key key){
    /**
     * 1）尝试从 readonly 中获取 key 对应 entry，若存在，则直接将 entry 中的内容置为 nullptr（软删除态）
     * 2）若 readonly 中 key 不存在，且 readonly.amended 为 false，则直接返回
     * 3）加锁
     * 4）再次执行一次步骤 1） 相同动作（并发场景下的 double check）
     * 5）若确定 readonly 中 key 不存在，从 dirty 中删除 key，并且执行 missLocked 流程
     */

    // 尝试从 readonly 中获取 key 对应 entry
    typename Map<Key,Value>::Entry::ptr e = nullptr;
    // 获取 readonly 引用，执行拷贝构造函数
    typename Map<Key,Value>::ReadOnly readonly = *this->m_readonly.load();
    auto it = readonly.m->find(key);
    if (it != readonly.m->end()){
        e = it->second;
    }

    // 如果 readonly 中 entry 存在，直接将 entry 中内容置为 nullptr 软删除态，并返回
    if (e != nullptr){
        e->evict();
        return;
    }

    // 若 readonly.amended 为 false，说明 dirty 中也不可能存在数据，直接返回
    if (!readonly.amended){
        return;
    }

    // 加锁
    Lock::lockGuard guard(this->m_dirtyLock);

    // 针对 readonly 进行 double check
    // 获取 readonly 引用，执行赋值函数
    readonly = *this->m_readonly.load();
    it = readonly.m->find(key);
    if (it != readonly.m->end()){
        e = it->second;
    }
    
    if (e != nullptr){
        e->evict();
        return;
    }

    if (!readonly.amended){
        return;
    }

    //  readonly 数据 miss，并且 amended 为 false，则需要从 dirty 中删除数据
    this->m_dirty.erase(key);
    // 执行 missLocked 流程
    this->missLocked();
}

/**
 * @brief: 遍历 map
 * @param：f——用于接收 key-value 对的闭包函数
 *            key——数据键  
 *            value——数据
 */
template <class Key, class Value>
void Map<Key,Value>::range(std::function<bool(const Key& key, const Value& value)> f){
    /**
     * 1）获取 readonly
     * 2）若 readonly.amended 为 true，加锁，然后将 dirty 中所有数据转移到 readonly 中
     * 3）遍历 readonly，依次执行闭包函数
     */
    typename Map<Key,Value>::ReadOnly readonly = *this->m_readonly.load();
    if (readonly.amended){
        Lock::lockGuard guard(this->m_dirtyLock);
        readonly = *this->m_readonly.load();
        if (readonly.amended){
            this->readonlyLocked();
        }
    }

    // 遍历 readonly
    readonly = *this->m_readonly.load();
    typename Map<Key,Value>::map& m = *readonly.m;
    for (auto it : m){
        Value v;
        if (!it.second->load(v)){
            continue;
        }
        if (!f(it.first,v)){
            break;
        }
    }
}

/**
 * @brief: 累计 read map 数据 miss 次数. 访问该方法时一定持有 dirtyLock
 * 当 miss 次数 >= dirty 大小时，需要使用 dirty 覆盖 read
 * 1) miss++  2) dirty -> read
 */
template<class Key, class Value>
void Map<Key,Value>::missLocked(){
    this->m_misses++;
    // 由于持有 dirtyLock，所以允许访问 dirty 的 size 方法
    if (this->m_misses.load() < this->m_dirty.size()){
        return;
    }
    // miss 次数过多，需要使用 dirty 覆盖 readonly，并清空原 dirty
    this->readonlyLocked();
}

/**
 * @brief：将 dirty 赋给 readonly，然后清空 dirty
 */
template <class Key, class Value>
void Map<Key,Value>::readonlyLocked(){
    // 从原子容器中获取 readonly 实例指针
    typename Map<Key,Value>::ReadOnly* readonly = this->m_readonly.load();
    // 将 dirty 通过移动函数填充到 readonly 中（通过 std::move 操作避免发生拷贝）
    readonly->m = std::make_shared<map>(std::move(this->m_dirty));
    // amended 标识置为 false
    readonly->amended = false;

    // 清空 readonly miss 计数器
    this->m_misses.store(0);
    // 构造新的 dirty 实例
    this->m_dirty = map();
}

/**
 * @brief: 倘若 dirty 为空，需要将 read 全量数据转移到 dirty 中
 * 遍历 read 时间复杂度 O(N)，同时清理掉处于硬删除态（expunged）的数据
 */
template<class Key, class Value>
void Map<Key,Value>::dirtyLocked(){
    if (!this->m_dirty.empty()){
        return;
    }

    // 获取 readonly 引用
    // 从 atomic 中获取 readonly 实例，并通过拷贝构造函数生成副本
    typename Map<Key,Value>::ReadOnly readonly = *this->m_readonly.load();
    // 获取 map 引用，不发生拷贝行为
    typename Map<Key,Value>::map& m = *readonly.m;
    this->m_dirty.reserve(m.size());
    /**
     * 遍历 map O(N)
     * 1）将软删除态 nullptr -> 硬删除态 expunged
     * 2）非删除态数据拷贝到 dirty 中
     */
    for (auto it : m){
        // 其中会将 readonly 中，原本为软删除态 nullptr 的 entry 置为硬删除态 expunged
        // 无论此前是软删除态还是硬删除态，都需要过滤该 entry，不添加到 dirty 中
        if (it.second->tryExpungeLocked()){
            continue;
        }
        // 对于 readonly 中未删除的数据，插入到 dirty 中
        this->m_dirty.insert({it.first,it.second});
    }
}

// entry 构造函数，入参为存储数据对应智能指针
template<class Key, class Value>
Map<Key,Value>::Entry::Entry(Value v){
    typename Map<Key,Value>::Entry::WrappedV* wv = new WrappedV(v);
    this->container.store(wv);
}

// entry 析构函数
template<class Key, class Value>
Map<Key,Value>::Entry::~Entry(){
    typename Map<Key,Value>::Entry::WrappedV* wv = this->container.load();
    if (wv == nullptr || wv == Map::s_expunged){
        return;
    }
    delete wv;
}

/**
 * 读操作相关
 */
/**
 * @brief: 从 entry 中读取数据，存储到 receiver 中
 * @param: 承载读取数据的容器
 * @return: true——读取成功 false——读取失败
 */
template<class Key, class Value>
bool Map<Key,Value>::Entry::load(Value& receiver){
    typename Map<Key,Value>::Entry::WrappedV* wv = this->container.load();
    if (wv == nullptr || wv == Map::s_expunged){
        return false;
    }

    receiver = wv->v;
    return true;
}

/**
 * entry 写操作相关
 */
/**
 * @brief: 存储数据 [调用此方法时一定处于持有锁状态]
 * @param: v——存储数据的智能指针
 * 进入该方法时，container 中的 old value 不可能为 expunged 态
 */
template<class Key, class Value>
void Map<Key,Value>::Entry::storeLocked(Value v){
    typename Map<Key,Value>::Entry::WrappedV* _new = new WrappedV(v);
    while(true){
        typename Map<Key,Value>::Entry::WrappedV* old = this->container.load();
        if (this->container.compare_exchange_strong(old,_new)){
            if (old != nullptr){
                delete old;
            }
            return;
        }
    }
}

/**
 * @brief: 尝试存储数据
 * @param: v——尝试存储数据的智能指针
 * @return: true——存储成功 false——存储失败（仅当处于硬删除态时失败）
 */
template<class Key, class Value>
bool Map<Key,Value>::Entry::tryStore(Value v){
    typename Map<Key,Value>::Entry::WrappedV* _new = new WrappedV(v);
    while (true){
        typename Map<Key,Value>::Entry::WrappedV* old = this->container.load();
        // 若此前已处于硬删除（expunged），则更新失败
        if (old == Map::s_expunged){
            delete _new;
            return false;
        }

        if (this->container.compare_exchange_strong(old,_new)){
            // 更新成功，需要删除原本的 wrappedV 实例
            if (old != nullptr){
                delete old;
            }
            return true;
        }
    }
}

// 删除 entry 中存储的数据
template<class Key, class Value>
void Map<Key,Value>::Entry::evict(){
    while (true){
        typename Map<Key,Value>::Entry::WrappedV* old = this->container.load();
        if ( old == nullptr || old == Map::s_expunged ){
            return;
        }
        if (this->container.compare_exchange_strong(old,nullptr)){
            delete old;
            return;
        }
    }
}

/**
 * entry 状态更新操作相关方法
 */
/**
 * @brief: 将 entry 置为非硬删除态
 * @return: true——之前处于硬删除态（expunged），更新成功；false——之前处于非硬删除态，无需更新
 */
template<class Key, class Value>
bool Map<Key,Value>::Entry::unexpungeLocked(){
    return this->container.compare_exchange_strong(Map::s_expunged,nullptr);
}

/**
 * @brief: 尝试将 entry 置为硬删除态（expunged）
 * @return: true——之前就是硬删除态，或者成功将 nil （软删除态）置为硬删除态； false——更新失败
 */
template<class Key, class Value>
bool Map<Key,Value>::Entry::tryExpungeLocked(){
    typename Map<Key,Value>::Entry::WrappedV* wv = this->container.load();
    while (wv == nullptr){
        if (this->container.compare_exchange_strong(wv, Map::s_expunged)){
            return true;
        }
        wv = this->container.load();
    }

    return wv == Map::s_expunged;
}

template<class Key, class Value>
Map<Key,Value>::Entry::WrappedV::WrappedV(Value v):v(v){}

// readonly 默认构造函数
template<class Key, class Value>
Map<Key,Value>::ReadOnly::ReadOnly():amended(false){
    this->m.reset(new map);
}

// readonly 拷贝构造函数重载
template<class Key, class Value>
Map<Key,Value>::ReadOnly::ReadOnly(const ReadOnly& other){
    this->amended = other.amended;
    this->m = other.m;
}

// readonly 赋值函数重载
template<class Key, class Value>
typename Map<Key,Value>::ReadOnly& Map<Key,Value>::ReadOnly::operator=(const ReadOnly& other){
    if (this == &other){
        return *this;
    }
    this->amended = other.amended;
    this->m = other.m;
    return *this;
}


}}