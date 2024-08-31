#include <exception>
#include <iostream>
#include <stdexcept>

#include "thread.h"
#include "../base/sys.h"

namespace cbricks{ namespace sync{

static thread_local Thread* t_thread = nullptr;

// cb——线程执行的函数 name——线程名称
Thread::Thread(std::function<void()> cb, const std::string name):m_cb(cb),m_name(name){
    // 创建线程
    if (pthread_create(&this->m_core,nullptr,&Thread::Fc,this)){
        throw std::exception();
    }
    this->m_sem.wait();
}

Thread::~Thread(){
    if (this->m_core){
        pthread_detach(this->m_core);
    }
}

// 获取线程 id
pid_t Thread::getId() const{
    return this->m_id;
}

// 获取线程名称
const std::string& Thread::getName() const{
    return this->m_name;
}

// 等待线程执行完成. 若提前 join 了，则会把 m_core 置为 null，析构时需要配合使用
void Thread::join(){
    if (this->m_core){
        pthread_join(this->m_core,nullptr);
        this->m_core = 0;
    }
}  

// 获取当前线程. 建议在线程池模式下使用此方法，否则一旦线程对象被析构，结果可能为空
Thread* Thread::GetThis(){
    return t_thread;
}

// 线程运行函数 schema，通过 static 风格，隐藏 this 指针
void* Thread::Fc (void* arg){
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    // 获取真实的当前线程 id
    thread->m_id = base::GetThreadId(); 
    std::function<void()> cb;
    cb.swap(thread->m_cb);
    thread->m_sem.notify();
    try{
        cb();
    }
    catch(std::exception& e){
        std::cout << e.what() << std::endl;
    }
    catch (...){

    }
    return 0;
}

}}