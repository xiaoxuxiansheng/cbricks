# cbricks
<p align="center">
<b>cbricks: 基于 c++ 11 实现的基本工具类</b>
<br/><br/>
</p>

<p align="center">
<img src="https://github.com/xiaoxuxiansheng/cbricks/blob/main/img/frame.png" height="400px/"><br/><br/>
<b>workerpool: 简易协程调度框架</b>
<br/><br/>
</p>

## 💡 技术博客
<a href="">`C++` 从零实现协程调度框架</a> <br/><br/>

## 🖥 接入 sop
- 协程调度框架 workerpool <br/><br/>
```cpp
#include <iostream>

#include "sync/sem.h"
#include "pool/workerpool.h"

void testWorkerPool();

int main(int argc, char** argv){
    // 测试函数
    testWorkerPool();
}

void testWorkerPool(){
    // 协程调度框架类型别名定义
    typedef cbricks::pool::WorkerPool workerPool;
    // 信号量类型别名定义
    typedef cbricks::sync::Semaphore semaphore;

    // 初始化协程调度框架，设置并发的 threads 数量为 8
    workerPool::ptr workerPoolPtr(new workerPool(8));  

    // 初始化一个原子计数器
    std::atomic<int> cnt{0};
    // 初始化一个信号量实例
    semaphore sem;

    // 投递 10000 个异步任务到协程调度框架中，执行逻辑就是对 cnt 加 1
    for (int i = 0; i < 10000; i++){
        // 执行 submit 方法，将任务提交到协程调度框架中
        workerPoolPtr->submit([&cnt,&sem](){
            cnt++;
            sem.notify();
        });
    }

    // 通过信号量等待 10000 个异步任务执行完成
    for (int i = 0; i < 10000; i++){
        sem.wait();
    }

    // 输出 cnt 结果（预期结果为 10000）
    std::cout << cnt << std::endl;
}
```
