#include "once.h"
#include "../base/defer.h"

namespace cbricks{namespace sync{

void Once::onceDo(std::function<void()>cb){
    // 执行过了，直接 return
    if (this->m_done.load()){
        return;
    }

    // 加锁 double check
    this->m_lock.lock();
    cbricks::base::Defer lockDefer([this](){this->m_lock.unlock();});

    if (this->m_done.load()){
        return;
    }

    cbricks::base::Defer doneDefer([this](){this->m_done.store(true);});

    try{
        if (cb){
            cb();
        }
    }
    catch(...)
    {

    }
}

}}