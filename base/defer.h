#pragma once 

#include <functional>

#include "nocopy.h"

namespace cbricks{namespace base{
// 利用栈变量析构函数机制，实现类似 go 的 defer 机制
class Defer : Noncopyable{
public:
    // 构造/析构函数
    Defer(std::function<void()> cb);
    ~Defer();

    // 清空函数内容
    void Clear();

private:
    std::function<void()> m_cb;
};
}}