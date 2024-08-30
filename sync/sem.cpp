#include <stdexcept>

#include "sem.h"

namespace cbricks{namespace sync{

Semaphore::Semaphore(int count){
     if(sem_init(&this->m_sem, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore(){
    sem_destroy(&this->m_sem);
}

void Semaphore::wait(){
    if(sem_wait(&this->m_sem)) {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify(){
    if(sem_post(&this->m_sem)) {
        throw std::logic_error("sem_notify error");
    }
}


}}