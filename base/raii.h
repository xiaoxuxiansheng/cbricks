#pragma once 

#include <functional>

#include "nocopy.h"

namespace cbricks{namespace base{
// resource accquisition in initialization
// 利用构造函数和析构函数完成指定闭包函数的逻辑
class RAII : Noncopyable{
public:
    RAII(std::function<void()> now, std::function<void()> defer);
    ~RAII();

private:
    std::function<void()> m_now;
    std::function<void()> m_defer;
};
}}