#pragma once 

#include <pthread.h>

#include "../base/nocopy.h"

namespace cbricks{namespace sync{

// 互斥锁. 不可进行值复制
class Lock : base::Noncopyable{
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