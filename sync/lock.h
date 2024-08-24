#pragma once 

#include <pthread.h>

#include "../base/nocopy.h"

namespace cbricks{namespace sync{

// 基于 RAII 风格，实现的互斥锁控制守卫
template <class Lock>
class LockGuard{
public:
    LockGuard(Lock& lock);    
    ~LockGuard();

private:
    Lock m_lock;
};

// 基于 RAII 风格，实现的读锁控制守卫
template <class ReadLock>
class ReadLockGuard{
public:
    ReadLockGuard(ReadLock& rlock);    
    ~ReadLockGuard();

private:
    ReadLock m_rlock;
};

// 互斥锁. 不可进行值复制
class Lock : base::Noncopyable{
public:
    // 类型别名
    typedef LockGuard<Lock> LockGuard;

public:
    // 构造/析构函数
    Lock();
    ~Lock();

public:
    // 公有的可操作方法
    void lock();
    void unlock();
    pthread_mutex_t* rawMutex();

private:
    // c 风格的互斥锁
    pthread_mutex_t m_mutex;
};


// 读写锁. 不可进行值复制
class RWLock : base::Noncopyable{
public:
    // 类型别名
    typedef LockGuard<Lock> WriteLockGuard;
    typedef ReadLockGuard<Lock> ReadLockGuard;

public:
    // 构造函数/析构函数
    RWLock();
    ~RWLock();

public:
    // 公有的可操作方法
    void rlock();
    void lock();
    void unlock();

private:
    // c 风格的读写锁
    pthread_rwlock_t m_rwMutex;
};

}}