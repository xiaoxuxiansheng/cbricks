#pragma once 

#include <atomic>
#include <pthread.h>

#include "../base/nocopy.h"
#include "scoped.h"

namespace cbricks{namespace sync{

// 互斥锁. 不可进行值复制
class Lock : base::Noncopyable{
public: 
    typedef ScopedLock<Lock> lockGuard;

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
    typedef ScopedLock<RWLock> lockGuard;
    typedef ScopedReadLock<RWLock> readLockGuard;

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

class SpinLock :base::Noncopyable{
public:
    typedef ScopedLock<SpinLock> lockGuard;

public:
    SpinLock();

public:
    void lock();
    void unlock();

private:
    std::atomic_flag m_locked;
};

}}