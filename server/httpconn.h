#pragma once 

#include <string>
#include <memory>
#include <unordered_map>

#include "../io/conn.h"
#include "../pool/instancepool.h"

namespace cbricks{namespace server{

// http 连接
class HttpConn : public pool::Instance{
public:
    // 智能指针 类型别名
    typedef std::shared_ptr<HttpConn> ptr;

public:
    /** 构造/析构 */
    // 构造函数，默认逻辑
    HttpConn() = default;
    // 析构函数，默认逻辑
    ~HttpConn() override = default;

public:
    // 初始化 http 连接. 传入原始请求内容，在构造函数中完成内容解析，填充各项内容，生成完备的 http conn
    void init(std::string& rawBody);
    // 清空实现 instance interface 方法
    void clear() override;

public:
    // 请求头
    struct Headers{
        const std::string& get(std::string& key) const;
    };

    // 路径请求参数
    struct Queries{
        const std::string& get(std::string& key) const;
    };

public:
    // 枚举值 http 方法
    enum Method{
        GET,
        POST
    };

public:
    /** 公开方法 */
    // 查询类
    // 获取这笔请求的方法
    const Method& getMethod() const;
    // 获取请求路径
    const std::string& getUrl() const;
    // 获取路径参数
    const Queries& getQueries() const;
    // 获取请求头
    const Headers& getHeaders() const;
    // 获取请求体
    const std::string& getBody() const;

    // 写入类
    // 写入响应内容
    void write(std::string& respData);

private:
    // 内部私有类. http 响应
    struct Response{
        std::string status; // e.g. "200 OK"
        int statusCode; // e.g. 200
        std::string proto; // e.g "HTTP/1.0"
        Headers header; // 请求头
        std::string body; // 内容
        // 组装生成原始
    };

private:
    // http 方法
    Method m_method;
    // 请求路径
    std::string m_url;
    // 请求路径参数
    Queries m_queries;    
    // 协议 "HTTP/1.0"
    std::string m_proto;
    // 请求头
    Headers m_headers;
    // 请求体
    std::string m_body;
    // 响应
    Response m_response;
};

}}