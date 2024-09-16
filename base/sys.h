#pragma once 

#include <unistd.h>
#include <sys/syscall.h>

namespace cbricks{namespace base{

pid_t GetThreadId();

/**
 * 针对指定 signal 注册回调函数
 */
void AddSignal(int sig, void(handler)(int));

}}