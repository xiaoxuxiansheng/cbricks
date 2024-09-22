#pragma once 

#include <string>
#include <memory>
#include <unordered_map>

#include "../io/conn.h"

namespace cbricks{namespace server{

// http 连接
class HttpConn{
public:
    // 智能指针 类型别名
    typedef std::shared_ptr<HttpConn> ptr;

public:
    /** 构造/析构 */
    // 构造函数，传入原始请求内容，在构造函数中完成内容解析
    HttpConn(std::string& rawBody);
    ~HttpConn() = default;

public:
    /** 公开方法 */
    // 获取方法
    std::string& getMethod();
    // 获取请求路径
    std::string& getUrl();
    // 获取路径参数
    std::unordered_map<std::string,std::string>& getQueries();
    // 获取请求头
    std::unordered_map<std::string,std::string>& getHeader();
    // 获取请求体
    std::string& getBody();

    // 写入响应内容
    void write(std::string& respData);

public:
    // 枚举值 http 方法
    enum Method{
        GET,
        POST
    };

private:
    // 内部私有类. http 响应
    struct Response{
        std::string status; // e.g. "200 OK"
        int statusCode; // e.g. 200
        std::string proto; // e.g "HTTP/1.0"
        std::unordered_map<std::string,std::string> header; // 请求头
        std::string body; // 内容
    };

private:
    // http 方法
    Method m_method;
    // 请求路径
    std::string m_url;
    // 请求路径参数
    std::unordered_map<std::string,std::string> m_queries;    
    // 协议 "HTTP/1.0"
    std::string m_proto;
    // 请求头
    std::unordered_map<std::string,std::string> m_header;
    // 请求体
    std::string m_body;
    // 响应
    Response m_response;
};

}}