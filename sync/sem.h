#pragma once 

#include <semaphore.h>

#include "../base/nocopy.h"

namespace cbricks{namespace sync{

class Semaphore : base::Noncopyable{
public:
    Semaphore(int count = 0);
    ~Semaphore();

    void wait();
    void notify();

private:
    sem_t m_sem;
};

}}