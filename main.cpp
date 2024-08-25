#include <iostream>
// #include <gtest/gtest.h>

#include "sync/lock.h"
#include "sync/thread.h"
#include "sync/coroutine.h"
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

int main(int argc, char** argv){
    // testThread();
    testCoroutine();
}

