#include <iostream>
#include <vector>
#include <string>
#include <exception>
#include <time.h>
#include <unistd.h>


#include "sync/lock.h"
#include "sync/thread.h"
#include "sync/coroutine.h"
#include "sync/queue.h"
#include "sync/channel.h"
#include "sync/sem.h"
#include "pool/workerpool.h"
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
                res.push_back(std::to_string(tmp));
                lock.unlock();
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

void testChannel(){
    typedef cbricks::sync::Channel<std::string> chan;
    typedef cbricks::sync::Thread thread;
    typedef cbricks::sync::Lock lock;
    chan ch(100);
    
    std::vector<thread::ptr> writers;
    std::vector<thread::ptr> readers;

    std::atomic<int> seed{0};
    for (int i = 0; i < 10; i++){
        thread::ptr tr(new thread([&ch,&seed](){
            std::vector<std::string> tmps;
            for (int i = 0; i < 10; i++){
                tmps.push_back(std::to_string(seed++));
            }
            if (!ch.writeN(tmps)){
                throw std::exception();
        }
        }));
        writers.push_back(tr);
    }

    std::vector<std::string> res;
    lock mutex;

    for (int i = 0; i < 20; i++){
        thread::ptr tr(new thread([&ch,&mutex, &res](){
            std::vector<std::string> tmps(5);
            if (!ch.readN(tmps)){
                throw std::exception();
            }
            mutex.lock();
            for (int i = 0; i < tmps.size(); i++){
                res.push_back(tmps[i]);
            }
            mutex.unlock();
        }));
        readers.push_back(tr);
    }

    for (int i = 0; i < 10; i++){
        writers[i]->join();
    }

    for (int i = 0; i < 20; i++){
        readers[i]->join();
    }

    for (int i = 0; i < res.size(); i++){
        std::cout << res[i] << std::endl;
    }

}

void testWorkerPool(){
    typedef cbricks::pool::WorkerPool workerPool;
    workerPool::ptr workerPoolPtr(new workerPool(8));  

    typedef cbricks::sync::Lock lock;
    typedef cbricks::sync::Semaphore semaphore;

    std::atomic<int> cnt{0};
    lock mutex;
    semaphore sem;

    for (int i = 0; i < 5000; i++){
        workerPoolPtr->submit([&cnt,&mutex,&sem](){
            mutex.lock();
            std::cout << cnt++ << std::endl;
            mutex.unlock();
            sem.notify();
        });
    }

    for (int i = 0; i < 5000; i++){
        sem.wait();
    }
}

int main(int argc, char** argv){
    // testThread();
    // testCoroutine();
    // testLinkedList();
    // testChannel();
    try{
        testWorkerPool();
    }
    catch(std::exception& e){
        std::cout << e.what() << std::endl;
    }
    catch(...){

    }

}

