#include <iostream>
#include <vector>
#include <string>
#include <exception>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <cstring>
#include <memory>


#include "sync/lock.h"
#include "sync/thread.h"
#include "sync/coroutine.h"
#include "sync/queue.h"
#include "sync/channel.h"
#include "sync/sem.h"
#include "sync/map.h"
#include "pool/instancepool.h"
#include "pool/workerpool.h"
#include "server/server.h"
#include "base/sys.h"
#include "base/defer.h"
#include "trace/assert.h"
#include "log/log.h"
// #include "mysql/conn.h"



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

void testAssert(){
    CBRICKS_ASSERT(false,"test test");
}

void testLog(){
    cbricks::log::Logger::Init("output/cbricks.log",50);
    for (int i = 0; i < 1000; i++){
        LOG_INFO("test case: %d",i);
    }

    sleep(1);
}

// void testMysql(){
//     cbricks::mysql::Conn conn;
// }

static const int PORT = 8080;

void testClient();

void testServer(){
    typedef cbricks::server::Server server;
    typedef cbricks::sync::Semaphore semaphore;
    typedef cbricks::sync::Thread thread;

    // 初始化日志模块
    cbricks::log::Logger::Init("output/cbricks.log",2000);
    // 信号量
    semaphore sem;
    // s.serve();

    // 异步启动 server
    thread t([&sem](){
        server s;
        std::function<std::string (std::string&)> cb = [](std::string& data)-> std::string{
            LOG_INFO("handle request: %s",data);
            return "success";
        };
        s.init(PORT,cb);
        s.serve();
        LOG_INFO("serve stop");
        sem.notify();
    });

    // // 睡 2秒确保 server 启动
    sleep(2);

    // 启动 n 个 client
    int n = 1000;
    for (int i = 0; i < n; i++){
        // 客户端并发请求
        thread client([&sem](){
            testClient();
            sem.notify();
        });
    }

    // 等待所有客户端线程退出
    for (int i = 0; i < n + 1; i++){
        sem.wait();
    }

    sleep(2);
}

void testClient(){
    // 初始化客户端 socket
    int clientSocket = socket(AF_INET,SOCK_STREAM,0);

    CBRICKS_ASSERT(clientSocket > 0,"init client socket failed");

    // 服务端 地址
    sockaddr_in serverAddr;
    std::memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 连接到服务器
    int ret = connect(clientSocket,(sockaddr*)&serverAddr,sizeof(serverAddr));
    // LOG_INFO("ret: %d, errno: %d",ret,errno);
    CBRICKS_ASSERT(ret ==  0,"connect server fail");

    // 发送请求
    std::string msg = "hello  request";
    send(clientSocket,msg.c_str(),msg.size(),0);

    // 接收响应
    char buffer[1024];
    std::memset(buffer,0,1024);
    int read = recv(clientSocket,buffer,1024,0);
    CBRICKS_ASSERT(read >  0,"client recv fail");

    std::string res(buffer,read);
    CBRICKS_ASSERT(res == "success","invalid response");
    close(clientSocket);
}

int testFc(){
    std::function<void(int)> f1 = [](int a){
        std::cout<<"test" << a <<std::endl;
    };
    // f1.operator()
    // // void(f2*)(int) = f1.;
    // f2(3);
}

void testSignal(){
    cbricks::base::AddSignal(SIGINT,[](int sig){
        std::cout << "capture sig: "<< sig << std::endl;
        std::exit(0);
    });
    cbricks::base::AddSignal(SIGHUP,[](int sig){
        std::cout << "capture sig: "<< sig << std::endl;
        std::exit(0);
    });
    cbricks::base::AddSignal(SIGTERM,[](int sig){
        std::cout << "capture sig: "<< sig << std::endl;
        std::exit(0);
    });
    cbricks::base::AddSignal(SIGKILL,[](int sig){
        std::cout << "capture sig: "<< sig << std::endl;
        std::exit(0);
    });
    cbricks::base::AddSignal(SIGQUIT,[](int sig){
        std::cout << "capture sig: "<< sig << std::endl;
        std::exit(0);
    });
    while(true){
        
    }
}

// void handle_sigint(int sig) {
//     std::cout << "Caught SIGINT, signal number = " << sig << std::endl;
//     // 可以在这里写入清理代码，例如释放资源等
//     std::exit(0);
// }
 
// int main() {
//     struct sigaction sa;
//     sa.sa_handler = &handle_sigint;
//     sigemptyset(&sa.sa_mask);
//     sa.sa_flags = 0;
//     sigaction(SIGINT, &sa, NULL);
 
//     while (true) {
//         sleep(1); // 用来模拟程序的运行，实际使用时可以替换为需要的代码
//     }
 
//     return 0;
// }



// demo——instance 实现类
class demo : public cbricks::pool::Instance{
public:
    // 默认构造函数
    demo() = default;
    // 默认析构函数
    ~demo() = default;
    // [必须实现] 置空函数
    void clear() override{
        LOG_INFO("clear...");
    }
    // 使用方法
    void print(){
        LOG_INFO("print...");      
    }
};

// 测试对象池代码示例
void testInstancePool(){
    // 对象实例 类型别名 
    typedef cbricks::pool::Instance instance;
    // 对象池 类型别名
    typedef cbricks::pool::InstancePool instancePool;
    // 信号量 类型别名
    typedef cbricks::sync::Semaphore semaphore;
    // 线程 类型别名
    typedef cbricks::sync::Thread thread;

    // 初始化日志模块，方便后续调试工作
    cbricks::log::Logger::Init("output/cbricks.log",5000);

    // 初始化对象池实例，声明对象构造函数
    instancePool pool([]()->instance::ptr{
        return instance::ptr(new demo);
    });

    // 信号量，用于作异步逻辑聚合，效果类似于 thread.join()
    semaphore sem;
    // 并发度
    int cnt = 10000;
    // 并发启动 thread
    for (int i = 0; i < cnt; i++){
        // thread 实例构造
        thread thr([&pool,&sem](){
            /**
             * thread 执行的异步逻辑：
             */
            // 1）从对象池中获取对象实例
            instance::ptr inst = pool.get();
            // 2）将对象实例转为指定类型
            std::shared_ptr<demo> d = std::dynamic_pointer_cast<demo>(inst);
            // 3）使用对象实例
            if (d){
                d->print();
            }
            // 4）归还对象实例
            pool.put(d);
            // 5）notify 信号量
            sem.notify();
        });
    }
    
    // 等待所有 thread 执行完成
    for (int i = 0; i < cnt; i++){
        sem.wait();
    }
}

void testSyncMap(){
    struct demo{
        int index;
        demo() = default;
        demo(int index):index(index){}
    };

    typedef cbricks::sync::Map<int,demo*> smap;
    typedef cbricks::sync::Semaphore semaphore;
    typedef cbricks::sync::Thread thread;

    // 并发写入 10 个 key，然后正常读取
    smap sm;
    semaphore sem;
    int cnt = 1000;
    int cnt2 = 500;
    std::atomic<int> flag1{0};
    std::atomic<int> flag2{0};
    std::atomic<int> flag3{0};

    for (int i = 0; i < cnt; i++){
        // 并发写 
        thread thr1([&sm,&sem,&flag1](){
            int index = (flag1++)/10;
            sm.store(index,new demo(index));
            sem.notify();
        });

        // 并发读
        thread thr2([&sm,&sem,&flag2](){
            int index = (flag2++)/10;
            demo* d = nullptr;
            sm.load(index,d);
            sem.notify();            
        });
    }

    for (int i = 0; i < cnt2; i++){
        // 并发删
        thread thr3([&sm,&sem,&flag3](){
            int index = (flag3++)/10;
            sm.evict(index);
            sem.notify();            
        });
    }

    for (int i = 0; i < 2 * cnt + cnt2; i++){
        sem.wait();
    }

    sm.range([](const int& key, const demo* d)->bool{
        int v = d->index;
        std::cout << "key: " << key << " , "<< "value " << v << std::endl;
        return true;
    });

    std::cout << "=========end=========" << std::endl;
}

int main(int argc, char** argv){
    // testThread();
    // testCoroutine();
    // testLinkedList();
    // testChannel();
    // testWorkerPool();
    // testAssert();
    // testLog();
    // testServer();
    // testFc();
    // testSignal();
    // testSyncMap();
}

