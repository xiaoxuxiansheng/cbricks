#pragma once 

#include <string>

namespace cbrick{ namespace log{

// 基于单例模式实现日志打印模块
class Logger{
public:
    // 静态方法获取单例
    static Logger* GetInstance(){
        // c++ 11 后，静态局部变量保证并发安全
        static Logger instance;
        return &instance;
    }

private:
    // 构造器私有化
    Logger();

private:
    // 日志文件绝对路径
    std::string m_logName;
    
};


}
}