#pragma once 

#include <functional>
#include <atomic>

#include "../base/nocopy.h"
#include "lock.h"

namespace cbricks{namespace sync{

class Once : base::Noncopyable{
public:
    // 构造/析构函数
    Once() = default;
    ~Once() = default;

public:
    // 公有操作函数. 保证传入的闭包只会被执行一次.
    void onceDo(std::function<void()>);

private:
    std::atomic<bool> m_done{false};
    Lock m_lock;
};

}}