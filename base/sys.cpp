#include <string.h>
#include <csignal>
#include <stddef.h>
#include <signal.h>

#include "sys.h"
#include "../trace/assert.h"

namespace cbricks{namespace base{

pid_t GetThreadId(){
    return syscall(SYS_gettid);
}

// 注册 signal 监听回调事件
void AddSignal(int sig, void(handler)(int)){
    std::signal(sig,handler);
}


}}