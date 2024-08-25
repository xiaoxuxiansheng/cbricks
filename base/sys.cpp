#include "sys.h"

namespace cbricks{namespace base{

pid_t GetThreadId(){
    return syscall(SYS_gettid);
}

}}