#pragma once 

#include <atomic>

#include "lock.h"
#include "cond.h"

namespace cbricks{namespace sync{

// 阻塞队列，并发通道
template <typename T>
class Channel{
public:
    // 数据类型
    typedef T dataType;

public:
    // 构造器函数
    Channel(const int cap = 1024);
    // 析构函数
    ~Channel();

public:
    // 操作函数. 
    // 往 channel 中推送数据. 如果 channel 满了，则陷入阻塞
    // ret——true 写入数据成功. ret——false 因为 channel 被关闭才被唤醒
    bool write(const T& data);
    // 从 channel 中读取数据. 如果 channel 是空的，则陷入阻塞
    // ret——true 读取数据成功. ret——false 因为 channel 被关闭才被唤醒
    bool read(T& receiver);

private:
    int roundTrip(const int);

private:
    // 并发控制
    Lock m_lock;
    Cond m_readCond;
    Cond m_writeCond;

    // 操作的数据结构. 通过线性表 + 循环双指针实现循环数组
    T* m_array;

    // 当前的数据量
    int m_size;
    // 总容量
    int m_cap;

    // 移动的双指针
    int m_front;
    int m_back;

    // channel 是否已关闭
    std::atomic<bool> m_closed{false};
    // 当前活跃的 reader 和 writer 总数
    std::atomic<int> m_activeCount{0};
};

}
}