#include <exception>

#include "channel.h"
#include "../base/raii.h"

namespace cbricks{namespace sync{

// 构造器函数
template <typename T>
Channel<T>::Channel(const int cap):m_cap(cap){
    if (this->m_cap <= 0){
        throw std::exception();
    }

    this->m_array = new T[cap];
    this->m_size = 0;
    this->m_front = -1;
    this->m_back = -1;
    this->m_activeCount = 0;
}

// 析构函数，需要唤醒所有的 writer 和 reader 之后，再进行退出
template <typename T>
Channel<T>::~Channel(){
    this->closed = true;
    
    // 唤醒所有的 writer 和 reader
    this->m_lock.lock();
    this->m_readCond.broadcast();
    this->m_writeCond.broadcast();
    this->m_lock.unlock();

    // 阻塞等待，直到所有 writer 和 reader 都正常退出
    while (this->m_activeCount){
        // ...
    }

    Lock::LockGuard(this->m_lock);
    if (this->m_array){
        delete[] this->m_array;
    }
}

// 操作函数. 
// 往 channel 中推送数据. 如果 channel 满了
// ret——true 操作成功；ret——false 操作失败，因超时返回
template <typename T>
bool Channel<T>::write(const T& data){
    if (this->m_closed){
        return false;
    }

    // 活跃计数器++
    this->m_lock.lock();

    if (this->m_closed){
        return false; 
    }

    RAII(
        [&this->m_activeCount](){this->m_activeCount++;},
        [&this->m_activeCount](){this->m_activeCount--;}
    );

    while (this->m_size == this->m_cap){
        this->m_writeCond.wait(this->m_lock);
        if (this->closed){
            this->m_lock.unlock();
            return false;
        }
    }

    // 写入数据
    this->m_back = this->roundTrip(this->m_back);
    this->m_array[this->m_back] = data;
    this->m_size++;

    // 写入成功后需要唤醒 reader 然后解锁
    this->m_readCond.broadcast();
    this->m_lock.unlock();
    return true;
}

// 从 channel 中读取数据. 如果 channel 是空的，最多阻塞 timeoutSeconds 秒
// ret——true 操作成功；ret——false 操作失败，因超时返回
template <typename T>
bool Channel<T>::read(T& receiver){
    if (this->m_closed){
        return false; 
    }

    this->m_lock.lock();

    if (this->m_closed){
        return false; 
    }

    RAII(
        [&this->m_activeCount](){this->m_activeCount++;},
        [&this->m_activeCount](){this->m_activeCount--;}
    );

    while (!this->m_size){
        this->m_readCond.wait(this->m_lock);
        if (this->closed){
            this->m_lock.unlock();
            return false;
        }
    }

    // 读取数据
    this->m_front = this->roundTrip(this->m_front);
    receiver = this->m_array[this->m_front];
    this->m_size--;

    // 读取成功后需要唤醒 writer 然后解锁
    this->m_writeCond.broadcast();
    this->m_lock.unlock();
    return true;    
}

template <typename T>
int Channel<T>::roundTrip(const int cur){
    return (cur + 1) % this->m_cap
}

}}
