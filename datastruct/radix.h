#pragma once 

#include <memory>
#include <string>
#include <vector>

namespace cbricks{namespace datastruct{

// 压缩前缀树
class RadixTree{
public:
    /**
     * 构造&析构
     */
    RadixTree();
    ~RadixTree();

private:
    /**
     * 内部嵌套类
     */
    /**
     * 压缩前缀树节点
     */
    struct Node{
        typedef std::shared_ptr<Node> ptr;

        // 节点的相对路径
        std::string path;
        // 节点的完整路径
        std::string fullPath;
        // 孩子节点首字母集合
        std::string indices;
        // 当前节点的优先级. 以当前节点为根的子树节点越多，优先级越高
        int priority;
        // 孩子节点
        std::vector<Node::ptr> children;
        // 节点存储的数据
        int value;
    };

public:
    /**
     * @brief：将 key-value 对写入 radix 
     * @param：key——键
     * @param：value——值
     */
    void put(std::string key, int value);

    /**
     * @brief：查询 key 对应 value
     * @param：key——键
     * @param：receiver——接收 value 的容器
     * @return：true——数据存在 false——数据不存在
     */
    bool get(std::string key, int& receiver);

    /**
     * @brief：查询 radix 中是否存在以 prefix 为前缀的 key
     * @param：指定前缀
     * @return：true——存在以 prefix 为前缀的 key ；false——不存在
     */
    bool prefix(std::string prefix);

private:
    // 根节点
    Node::ptr m_root;
};


/**
 * @brief：将 key-value 对写入 radix 
 * @param：key——键
 * @param：value——值
 */
void RadixTree::put(std::string key, int value){


}

/**
 * @brief：查询 key 对应 value
 * @param：key——键
 * @param：receiver——接收 value 的容器
 * @return：true——数据存在 false——数据不存在
 */
bool RadixTree::get(std::string key, int& receiver){
    
}

/**
 * @brief：查询 radix 中是否存在以 prefix 为前缀的 key
 * @param：指定前缀
 * @return：true——存在以 prefix 为前缀的 key ；false——不存在
 */
bool RadixTree::prefix(std::string prefix){
    
}


}}