#include "httpserver.h"

namespace cbricks{namespace server{
/**
 * @brief: http server 构造函数
 * @param: port——服务启动端口号
 * @param: threads——运行的线程数
 * @param: maxRequest——最大同时处理的请求数
*/
HttpServer::HttpServer(int port, const int threads, const int maxRequest){
    /**
     * 1）完成路由树等数据结构的初始化
     * 2）完成父类 server 的初始化
    */
}

// serve 启动服务
void HttpServer::serve(){

}

// 路由模块相关
/**
 * 注册 middleware
 */
void HttpServer::use(std::string path, middleware mw){
    // 添加到路由树中
}

/**
 * 路由注册 GET 方法
*/
void HttpServer::GET(std::string path, handler fc){
    // 添加到路由树中
}

/**
 * 路由注册 POST 方法
*/
void HttpServer::POST(std::string path, handler fc){
    // 添加到路由树中
}

}}