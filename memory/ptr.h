#pragma once 

#include <atomic>

namespace cbricks{ namespace memory{

// 共享智能指针
template <typename T>
class SharedPtr{
public:
    /**
     * 构造&析构函数
     */
    // 基础构造函数
    explicit SharedPtr(T* ptr = nullptr);
    // 拷贝构造函数
    SharedPtr(const SharedPtr<T>& other);
    // 析构函数
    ~SharedPtr();

public:
    /**
     * 公有方法
     */
    // = 赋值操作符重载
    SharedPtr<T>& operator=(const SharedPtr<T>& other);
    // * 操作符重载
    T& operator*();
    // -> 操作符重载
    T* operator->();
    T* get();

    // 重置指针
    void reset();
    void reset(T* ptr);
    // 当前实例被多少共享指针引用
    int use_count();

private:
    void release();    

private:
    T* m_ptr;
    std::atomic<int>* m_cnt;
};

/**
 * 构造&析构函数
 */
// 基础构造函数
template<typename T>
SharedPtr<T>::SharedPtr(T* ptr):m_ptr(ptr),m_cnt(ptr != nullptr ? new std::atomic<int>(1) : nullptr){}

// 拷贝构造函数
template<typename T>
SharedPtr<T>::SharedPtr(const SharedPtr<T>& other):m_ptr(other.m_ptr),m_cnt(other.m_cnt){
    if (this->m_cnt == nullptr){
        return;
    }
    ++(*this->m_cnt);
}

// 析构函数
template<typename T>
SharedPtr<T>::~SharedPtr(){
    this->release();
}

// 赋值操作符重载
template<typename T>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr<T>& other){
    if (this == &other){
        return *this;
    }

    this->release();
    this->m_ptr = other.m_ptr;
    this->m_cnt = other.m_cnt;
    if (this->m_cnt == nullptr){
        return *this;
    }
    ++(*this->m_cnt);
    return *this;
}

// * 操作符重载
template<typename T>
T& SharedPtr<T>::operator*(){
    return *this->m_ptr;
}

// -> 操作符重载
template<typename T>
T* SharedPtr<T>::operator->(){
    return this->m_ptr;
}

template<typename T>
T* SharedPtr<T>::get(){
    return this->m_ptr;
}

// 重置指针
template<typename T>
void SharedPtr<T>::reset(){
    this->release();
    this->m_ptr = nullptr;
    this->m_cnt = nullptr;
}

template<typename T>
void SharedPtr<T>::reset(T* ptr){
    this->release();
    this->m_ptr = ptr;
    this->m_cnt = new std::atomic<int>(1);
}

template<typename T>
int SharedPtr<T>::use_count(){
    if (this->m_cnt == nullptr){
        return 0;
    }
    return this->m_cnt->load();
}

template<typename T>
void SharedPtr<T>::release(){
    if (this->m_cnt == nullptr){
        return;
    }
    if (--(*this->m_cnt) > 0){
        return;
    }
    delete this->m_ptr;
    delete this->m_cnt;
}

template<typename T>
class WeakPtr{
public:
    /**
     * 构造 析构
     */
    WeakPtr(const SharedPtr<T>& other);
    WeakPtr(const WeakPtr<T>& other);
    ~WeakPtr() = default;

public:
    // 公有方法
    WeakPtr<T>& operator=(const WeakPtr<T>& other);
    // 判断观察的对象是否过期
    bool expired();
    // 获取观察的对象
    SharedPtr<T> lock();

private:
    SharedPtr<T>& m_ptr;
};

template <typename T>
WeakPtr<T>::WeakPtr(const SharedPtr<T>& other):m_ptr(other){}

template <typename T>
WeakPtr<T>::WeakPtr(const WeakPtr<T>& other):m_ptr(other.m_ptr){}

template <typename T>
WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr<T>& other){
    if (this == &other){
        return *this;
    }

    this->m_ptr = other.m_ptr;
    return *this;
}

template <typename T>
bool WeakPtr<T>::expired(){
    return this->m_ptr == nullptr;
}

template <typename T>
SharedPtr<T> WeakPtr<T>::lock(){
    return nullptr;
}




}}