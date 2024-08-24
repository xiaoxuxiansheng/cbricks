#pragma once 

#include <pthread.h>

#include "lock.h"
#include "../base/nocopy.h"

namespace cbricks{ namespace sync{

class Cond : base::Noncopyable{
public:
    // 构造/析构函数
    Cond();
    ~Cond();

public:
    // 公有操作函数
    bool wait(Lock& lock);
    bool signal();
    bool broadcast();

private:
    pthread_cond_t m_cond;
};

}} 
