#pragma once 

#include <atomic>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

#include "../base/nocopy.h"
#include "../base/defer.h"
#include "lock.h"
#include "cond.h"

namespace cbricks{namespace sync{

// 阻塞队列，并发通道
template <typename T>
class Channel : base::Noncopyable {
public:
    // 共享智能指针类型别名
    typedef std::shared_ptr<Channel<T>> ptr;

public:
    // 构造器函数.
    Channel(const int cap = 1024);
    // 析构函数
    ~Channel();

public:
    // 操作函数. 
    // 往 channel 中推送数据. 如果 channel 满了，则陷入阻塞
    // ret——true 写入数据成功. false 写入数据失败
    bool write(const T data, bool nonblock = false);
    bool writeN(std::vector<T> datas, bool nonblock = false);

    // 从 channel 中读取数据. 如果 channel 是空的，则陷入阻塞
    // ret——true 读取数据成功. false 写入数据失败
    bool read(T& receiver, bool nonblock = false);
    bool readN(std::vector<T>& receivers, bool nonblock = false);

    // 内部数据是否为空
    const bool empty();
    const int size();
    const int cap();

    // 主动关闭 channel
    void close();

private:
    int roundTrip(int index);

private:
    // 并发控制
    Lock m_lock;
    Cond m_readCond;
    Cond m_writeCond;
    
    int m_front;
    int m_back;
    int m_size;

    // 操作的数据结构. 通过线性表 + 循环双指针实现环形数组
    std::vector<T> m_array;

    // channel 是否已关闭
    std::atomic<bool> m_closed{false};

    // 记录当前活跃的 reader 和 writer 总数
    std::atomic<int> m_subscribers{0};
};

// 构造器函数
template <typename T>
Channel<T>::Channel(const int cap){
    if (cap <= 0){
        throw std::exception();
    }

    this->m_array = std::vector<T>(cap);
    this->m_front = -1;
    this->m_back = -1;
    this->m_size = 0;
}

// 析构函数，需要唤醒所有的 writer 和 reader 之后，再进行退出
template <typename T>
Channel<T>::~Channel(){
    this->close();
}

template <typename T>
void Channel<T>::close(){
    // 1 将 closed 标记为 true，保证不再生成新的 writer 和 reader
    if (this->m_closed){
        return;
    } 
    
    // 2 唤醒所有的 writer 和 reader
    {
        this->m_lock.lock();
        cbricks::base::Defer defer([this](){this->m_lock.unlock();});

        if (this->m_closed){
            return;
        } 
        this->m_closed = true;
        
        this->m_readCond.broadcast();
        this->m_writeCond.broadcast();
    }

    // 3 阻塞等待，直到所有 writer 和 reader 都正常退出
    while (this->m_subscribers){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 往 channel 中推送数据. 如果 channel 满了，则根据阻塞模式来判定是陷入阻塞还是直接返回 false
// ret——true 操作成功；ret——false 操作失败
template <typename T>
bool Channel<T>::write(const T data, bool nonblock){
    std::vector<T> datas;
    datas.push_back(data);
    return this->writeN(datas,nonblock);
}

template <typename T>
bool Channel<T>::writeN(std::vector<T> datas, bool nonblock){
    if (this->m_closed){
        return false;
    }

    // 加锁
    this->m_lock.lock();
    cbricks::base::Defer lockDefer([this](){this->m_lock.unlock();});

    // 已关闭
    if (this->m_closed){
        return false; 
    }

    this->m_subscribers++;
    cbricks::base::Defer suscribeDefer([this](){this->m_subscribers--;});

    // 如果容量已满
    while (this->m_size + datas.size() > this->m_array.size()){
        // 非阻塞模式，直接退出
        if (nonblock){
            return false;
        }

        // 阻塞模式，则陷入等待
        this->m_writeCond.wait(this->m_lock);

        // 已关闭，直接退出
        if (this->m_closed){
            return false;
        }
    }

    // 写入数据
    for (int i = 0; i < datas.size(); i++){
        this->m_back = this->roundTrip(this->m_back);
        this->m_array[this->m_back] = datas[i];
        this->m_size++;
    }

    // 写入成功后需要唤醒 reader 然后解锁返回
    this->m_readCond.signal();
    return true;
}

// 从 channel 中读取数据. 如果 channel 是空的
// ret——true 操作成功；ret——false 操作失败
template <typename T>
bool Channel<T>::read(T& receiver, bool nonblock){
    std::vector<T>receivers(1);
    if (!this->readN(receivers,nonblock)){
        return false;
    }  
    receiver = receivers[0];
    return true;
}

template <typename T>
bool Channel<T>::readN(std::vector<T>& receivers, bool nonblock){
    if (this->m_closed){
        return false; 
    }

    this->m_lock.lock();
    cbricks::base::Defer lockDefer([this](){this->m_lock.unlock();});

    if (this->m_closed){
        return false; 
    }

    this->m_subscribers++;
    cbricks::base::Defer subscribeDefer([this](){this->m_subscribers--;});

    while (this->m_size < receivers.size()){
        if (nonblock){
            return false;
        }

        this->m_readCond.wait(this->m_lock);
        if (this->m_closed){
            return false;
        }
    }

    // 读取数据
    for (int i = 0; i < receivers.size(); i++){
        this->m_front = this->roundTrip(this->m_front);
        receivers[i] = this->m_array[this->m_front];
        this->m_size--;
    }

    // 读取成功后需要唤醒 writer 然后解锁
    this->m_writeCond.signal();
    return true;
}

template <typename T>
int Channel<T>::roundTrip(const int cur){
    return (cur + 1) % this->m_array.size();
}

template <typename T>
const bool Channel<T>::empty(){
    this->m_lock.lock();
    cbricks::base::Defer lockDefer([this](){this->m_lock.unlock();});
    return this->m_size == 0;
}

template <typename T>
const int Channel<T>::size(){
    this->m_lock.lock();
    cbricks::base::Defer lockDefer([this](){this->m_lock.unlock();});
    return this->m_size;
}

template <typename T>
const int Channel<T>::cap(){
    return this->m_array.size();
}

}}