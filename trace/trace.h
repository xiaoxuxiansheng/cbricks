#pragma once 

#include <string>
#include <vector>

namespace cbricks{namespace trace{

/**
 * 获取当前栈信息，并转换成字符串
 * size——最大栈层数
 * skip——跳过的栈层数
 * prefix——输出信息的前缀文本内容
 * 
 */
std::string BacktraceToString(int size = 64, int skip = 1, const std::string& prefix = "  ");

/**
 *  获取当前调用栈信息，存储在 receiver 中
 *  receiver——存储栈信息的容器
 *  size——最大栈层数
 *  skip——跳过的栈层数
 */
void BackTrace(std::vector<std::string>& receiver, int size = 64, int skip = 1);

}} 
