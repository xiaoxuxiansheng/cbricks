#include <iostream>
#include <vector>
#include <string>

#include "sync/lock.h"
#include "sync/thread.h"
#include "sync/coroutine.h"
#include "sync/queue.h"
#include "base/sys.h"
#include "base/defer.h"

void testThread(){
    pid_t threadId1 = 0;
    pid_t threadId2 = 0;

    cbricks::sync::Thread::ptr threadPtr1(new cbricks::sync::Thread([&threadId1]{
       threadId1 = cbricks::base::GetThreadId();
    },"test1"));

    cbricks::sync::Thread::ptr threadPtr2(new cbricks::sync::Thread([&threadId2]{
        threadId2 = cbricks::base::GetThreadId();
    },"test2"));

    threadPtr1->join();
    threadPtr2->join();

    std::cout << "threadId1: " << threadId1 << "; " << "threadName1: " << threadPtr1->getName() << std::endl;
    std::cout << "threadId2: " << threadId2 << "; " << "threadName2: " << threadPtr2->getName() << std::endl;
    // ASSERT_EQ(threadId1,threadPtr1->getId());
    // ASSERT_EQ(threadId2,threadPtr2->getId());
    // ASSERT_EQ("test1",threadPtr1->getName());
    // ASSERT_EQ("test2",threadPtr2->getName());
    // ASSERT_NE(threadId1,threadId2);
}

void testCoroutine(){
    cbricks::sync::Lock lock;
    
    std::function<void()> cb = [&lock](){
        lock.lock();
        uint64_t mainId = cbricks::sync::Coroutine::GetMain()->getId();
        uint64_t workerId = cbricks::sync::Coroutine::GetThis()->getId();
        std::cout << "main coroutine id: " << mainId << std::endl;
        std::cout << "worker coroutine id: " << workerId << std::endl;
        lock.unlock();
        cbricks::sync::Coroutine::GetThis()->sched();
        lock.lock();
        std::cout << "==========================="<< std::endl;
        std::cout << "[after sched] main coroutine id: " << mainId << std::endl;
        std::cout << "[after sched] worker coroutine id: " << workerId << std::endl;
        lock.unlock();
    };

    cbricks::sync::Coroutine coroutine1(cb);
    cbricks::sync::Coroutine coroutine2(cb);

    coroutine1.go();
    coroutine2.go();
    coroutine1.go();
    coroutine2.go();

    std::cout << "success..." << std::endl;
}

void testLinkedList(){
    cbricks::sync::Queue<int>::ptr queue(new cbricks::sync::Queue<int>());

    // 异步启动 100 个线程写入节点
    std::vector<cbricks::sync::Thread::ptr> writers;
    
    std::atomic<int> flag{0};
    for (int i = 0; i < 100; i++){
        cbricks::sync::Thread::ptr thread(new cbricks::sync::Thread([&queue,&flag]{
            queue->push(flag++);
        }));
        writers.push_back(thread);
    }

    // 异步启动 100 个线程弹出节点
    std::vector<std::string> res;
    cbricks::sync::Lock lock;


    std::vector<cbricks::sync::Thread::ptr> readers;
    for (int i = 0; i < 100; i++){
        cbricks::sync::Thread::ptr thread(new cbricks::sync::Thread([&queue,&lock,&res]{
            int tmp;
            if (queue->pop(tmp)){
                lock.lock();
                cbricks::base::Defer defer([&lock](){lock.unlock();});
                res.push_back(std::to_string(tmp));
            }
        }));
        readers.push_back(thread);
    }   

    for (int i = 0; i < writers.size(); i++){
        writers[i]->join();
    }

    for (int i = 0; i < readers.size(); i++){
        readers[i]->join();
    }

    // int tmp;
    // while(list->pop(tmp)){
    //     res.push_back(std::to_string(tmp));
    // }


    for (int i = 0; i < res.size(); i++){
        std::cout<< res[i] << std::endl;
    }
}

int main(int argc, char** argv){
    // testThread();
    // testCoroutine();
    testLinkedList();
}

