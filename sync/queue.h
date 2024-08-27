#pragma once 

#include <memory>
#include <atomic>

#include "../base/nocopy.h"
#include "../base/defer.h"

namespace cbricks{namespace sync{

// 并发安全的双向链表结构，只支持按照队列模式，从对头插入元素，从队尾弹出元素
template <typename T>
class Queue : base::Noncopyable{
public: 
    typedef std::shared_ptr<Queue<T>> ptr;

public:
    Queue();
    ~Queue();

public:
    void push(T data);
    bool pop(T& data);

private:
    class ListNode{   
    public:
        ListNode() = default;
        ListNode(T data);

        sync::SpinLock lock;
        
        T data;

        // 相邻节点
        std::shared_ptr<ListNode> prev;
        std::shared_ptr<ListNode> next;
    };

    // 头结点
    std::shared_ptr<ListNode> m_head; 
    // 尾节点
    std::shared_ptr<ListNode> m_tail;
};

// 构造函数
template<typename T>
Queue<T>::Queue(){
    this->m_head.reset(new ListNode);
    this->m_tail.reset(new ListNode);
    
    this->m_head->next = this->m_tail;
    this->m_head->prev = nullptr;
    this->m_tail->prev = this->m_head;
    this->m_tail->next = nullptr;
}

// 析构时将各节点之间的引用指针删除即可
template<typename T>
Queue<T>::~Queue(){
    // 遍历所有节点，删除对应的指针
    std::shared_ptr<ListNode> move = this->m_head;

    while (move){
        std::shared_ptr<ListNode> next = move->next;
        move->next = nullptr;
        move->prev = nullptr;
        move = next;
    }
}

template<typename T>
void Queue<T>::push(T data){
    // 对 head 加锁，完成节点插入
    this->m_head->lock.lock();
    base::Defer defer([this](){
        this->m_head->lock.unlock();
    });


    std::shared_ptr<ListNode> cur(new ListNode(data));
    // 节点的 next 
    cur->next = this->m_head->next;
    cur->prev = this->m_head;

    this->m_head->next->prev = cur;
    this->m_head->next = cur;
}

template<typename T>
bool Queue<T>::pop(T& data){
    // 先针对尾节点加锁
    this->m_tail->lock.lock();
    base::Defer tailDefer([this](){
        this->m_tail->lock.unlock();
    });

    // 再针对拟删除节点的前驱节点加锁
    std::shared_ptr<ListNode> lockTarget = this->m_tail->prev->prev;
    if (!lockTarget){
        return false;
    }

    lockTarget->lock.lock();
    base::Defer targetDefer([&lockTarget](){
        lockTarget->lock.unlock();
    });

    std::shared_ptr<ListNode> targetPrev = lockTarget;
    while (targetPrev->next->next != this->m_tail){
        targetPrev = targetPrev->next;
    }

    std::shared_ptr<ListNode> target = targetPrev->next;
    data = target->data;

    this->m_tail->prev = targetPrev;
    targetPrev->next = this->m_tail;

    target->prev = nullptr;
    target->next = nullptr;
    
    return true;
}

template<typename T>
Queue<T>::ListNode::ListNode(T data):data(data),prev(nullptr),next(nullptr){}

}}