#pragma once 

#include <memory>
#include <functional>

#include "server.h"
#include "httpconn.h"

namespace cbricks{namespace server{

// http server 继承 epoll server
class HttpServer : Server{
public:
    // 智能指针 类型别名
    typedef std::shared_ptr<HttpServer> ptr;
    typedef std::shared_ptr<HttpConn> connPtr;
    // 路由处理函数
    typedef std::function<void(connPtr)> handler;
    typedef handler middleware;

public:
    // 构造/析构函数
    /**
     * @brief: http server 构造函数
     * @param: port——服务启动端口号
     * @param: threads——运行的线程数
     * @param: maxRequest——最大同时处理的请求数
     */
    explicit HttpServer(int port, const int threads = 8, const int maxRequest = MAX_REQUEST);
    ~HttpServer() override = default;

public:
    // serve 启动服务
    void serve() override;

public:
    // 路由模块相关
    /**
     * 注册 middleware
     */
    void use(std::string path, middleware mw);
    /**
     * 路由注册 GET 方法
     */
    void GET(std::string path, handler fc);
    /**
     * 路由注册 POST 方法
     */
    void POST(std::string path, handler fc);
};


}}