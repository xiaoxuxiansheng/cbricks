#include "lock.h"

namespace cbricks{namespace sync{

template <class Lock> 
LockGuard<Lock>::LockGuard(Lock& lock):m_lock(lock){
    this->m_lock.lock();
}  

template <class Lock>
LockGuard<Lock>::~LockGuard(){
    this->m_lock.unlock();
}

template <class ReadLock>
ReadLockGuard<ReadLock>::ReadLockGuard(ReadLock& rlock):m_rlock(rlock){
    this->m_rlock.rlock();
}  

template <class ReadLock>
ReadLockGuard<ReadLock>::~ReadLockGuard(){
    this->m_rlock.unlock();
}

Lock::Lock(){
    pthread_mutex_init(&this->m_mutex,nullptr);
}

Lock::~Lock(){
    pthread_mutex_destroy(&this->m_mutex);
}

void Lock::lock(){
    pthread_mutex_lock(&this->m_mutex);
}

void Lock::unlock(){
    pthread_mutex_unlock(&this->m_mutex);
}

pthread_mutex_t* Lock::rawMutex(){
    return &this->m_mutex;
}

RWLock::RWLock(){
    pthread_rwlock_init(&this->m_rwMutex,nullptr);
}

RWLock::~RWLock(){
    pthread_rwlock_destroy(&this->m_rwMutex);
}

// 加写锁
void RWLock::lock(){
    pthread_rwlock_wrlock(&this->m_rwMutex);
}

// 加读锁
void RWLock::rlock(){
    pthread_rwlock_wrlock(&this->m_rwMutex);
}

// 解锁
void RWLock::unlock(){
    pthread_rwlock_unlock(&this->m_rwMutex);
}

}}
