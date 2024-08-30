#pragma once 

#include "../base/nocopy.h"

namespace cbricks{ namespace sync{

template <class Lock>
class ScopedLock :base::Noncopyable{
public:
    ScopedLock(Lock& lock):m_lock(lock){
        this->m_lock.lock();
    }

    ~ScopedLock(){
        this->m_lock.unlock();
    }

private:
    Lock& m_lock;
};

template <class ReadLock>
class ScopedReadLock :base::Noncopyable{
public:
    ScopedReadLock(ReadLock& lock):m_readlock(lock){
        this->m_readlock.lock();
    }

    ~ScopedReadLock(){
        this->m_readlock.unlock();
    }

private:
    ReadLock& m_readlock;
};


}}