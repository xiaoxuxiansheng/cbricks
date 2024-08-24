#include "cond.h"

namespace cbricks{namespace sync{

Cond::Cond(){
    pthread_cond_init(&this->m_cond,nullptr);
}

Cond::~Cond(){
    pthread_cond_destroy(&this->m_cond);
}

// 公有操作函数
bool Cond::wait(Lock& lock){
    return pthread_cond_wait(&this->m_cond,lock.rawMutex()) == 0;
}

bool Cond::signal(){
    return pthread_cond_signal(&this->m_cond);
}

bool Cond::broadcast(){
    return pthread_cond_broadcast(&this->m_cond);
}

}}
