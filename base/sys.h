#pragma once 

#include <unistd.h>
#include <sys/syscall.h>

namespace cbricks{namespace base{

pid_t GetThreadId();

}}