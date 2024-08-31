#include <sstream>
#include <cxxabi.h>
#include <execinfo.h>
#include <stdexcept>

#include "trace.h"

namespace cbricks{namespace trace{

/**
 * 获取当前栈信息，并转换成字符串
 * size——最大栈层数
 * skip——跳过的栈层数
 * prefix——输出信息的前缀文本内容
 * 
 */
std::string BacktraceToString(int size, int skip, const std::string& prefix){
    // 构造存储信息的容器
    std::vector<std::string> receiver;
    BackTrace(receiver,size,skip);

    std::stringstream ss;
    for (int i = 0; i < receiver.size(); i++){
        ss << prefix << receiver[i] << std::endl;
    }
    return ss.str();
}

static std::string demangle(const char* str);

/**
 *  获取当前调用栈信息，存储在 bt 中
 *  bt——存储栈信息的容器
 *  size——最大栈层数
 *  skip——跳过的栈层数
 */
void BackTrace(std::vector<std::string>& receiver, int size, int skip){
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s = ::backtrace(array,size);

    char** strings = backtrace_symbols(array,s);
    if (!strings){
        throw std::runtime_error("empty backtrace string");
    }

    for (size_t i = skip; i < s; i++){
        receiver.push_back(demangle(strings[i]));
    }

    free(strings);
    free(array);
}  

static std::string demangle(const char* str) {
    size_t size = 0;
    int status = 0;
    std::string rt;
    rt.resize(256);
    if(1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
        char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if(v) {
            std::string result(v);
            free(v);
            return result;
        }
    }
    if(1 == sscanf(str, "%255s", &rt[0])) {
        return rt;
    }
    return str;
}

}} 
