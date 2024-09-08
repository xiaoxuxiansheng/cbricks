#pragma once 

#include <chrono>
#include <pthread.h>

#include "lock.h"
#include "../base/nocopy.h"

namespace cbricks{ namespace sync{

class Cond : base::Noncopyable{
public:
    typedef std::chrono::seconds seconds;

public:
    // 构造/析构函数
    Cond();
    ~Cond();

public:
    // 公有操作函数
    bool wait(Lock& lock);
    bool waitFor(Lock& lock, seconds secs);
    bool signal();
    bool broadcast();

private:
    pthread_cond_t m_cond;
};

}} 
