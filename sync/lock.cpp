#include "lock.h"

namespace cbricks{namespace sync{

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

SpinLock::SpinLock():m_locked(ATOMIC_FLAG_INIT){}

void SpinLock::lock(){
    while(this->m_locked.test_and_set(std::memory_order_acquire)){}
}

void SpinLock::unlock(){
    this->m_locked.clear(std::memory_order_release);
}

}}
