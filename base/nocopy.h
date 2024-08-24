#pragma once 

namespace cbricks{namespace base{
    
class Noncopyable{
public:
    // 默认构造函数
    Noncopyable() = default;

    // 默认析构函数
    ~Noncopyable() = default;

    // 禁用拷贝构造函数
    Noncopyable(const Noncopyable&) = delete; 

    // 禁用赋值函数
    Noncopyable& operator=(const Noncopyable&) = delete;
};


}}