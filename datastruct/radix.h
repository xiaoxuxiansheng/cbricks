#pragma once 

#include <memory>
#include <string>
#include <vector>

namespace cbricks{namespace datastruct{

/**
 * 基于 c++ 实现压缩前缀树
 */
template <class T>
class RadixTree{
public:
    /**
     * 构造函数——完成 root 节点初始化
     */
    RadixTree();

private:
    /**
     * 内部类 Node: 前缀树中的节点
     */
    struct Node{
        // 智能指针类型别名
        typedef std::shared_ptr<Node> ptr;
        // 节点类型
        enum Type{
            // 标识当前节点中不存储 value. 当前节点的拆分仅因包含了多个子节点的公共前缀
            STATIC,
            // 标识当前节点中存储了 value
            PARAM
        };

        // 节点类型
        Type nodeType;
        // 相对路径
        std::string path;
        // 完整路径，为从根节点出发直至抵达当前节点后，途径所有节点的相对路径之和
        std::string fullPath;
        // 子节点的首字母集合，类似于索引
        std::string indices;
        // 子节点列表，各个子节点与 indices 中的顺序一一对应
        std::vector<Node::ptr> children;
        // 节点优先级. 子节点数越多，优先级越高
        int priority;
        // 节点存储的 value
        T value;

        // 构造函数
        Node(Type typ);

        /**
         * @brief 向节点中插入 value 值，该方法会将节点类型更新为 PARAM
         * @param：path——节点相对路径
         * @param：fullPath——节点完整路径
         * @param：value——存储值
         */
        void insert(const std::string& path, const std::string& fullPath, T value);
        /** 
         * 获取 path 和当前节点相对路径—— node.path 的公共前缀的长度
        */
        int longestCommonPrefix(const std::string& path) const;
        /**
         * 1）增加 children[pos] 子节点的 priority
         * 2）调整 children 中子节点的顺序，保证按照 priority 从高到底进行排序
         * 3）若 children 顺序发生了变化，则对 indices 执行相应的调整
         * 4）返回 children[pos] 在调整顺序后新的 index
         */
        int incrementChildPrio(const int pos);
        // 追加一个新的子节点
        void addChild(Node::ptr child);
    };

public:
    /**
     * @brief：将 key-value 对写入 radix tree
     * @param：key——键
     * @param：value——值
     */
    void put(const std::string& key, T value);

    /**
     * @brief：查询 key 对应 value
     * @param：key——键
     * @param：receiver——接收 value 的容器
     * @return：true——数据存在 false——数据不存在
     */
    bool get(const std::string& key, T& receiver);

private:
    /**
    * 获取 key 对应的 radix node
    */
    typename Node::ptr get(const std::string& key);

private:
    // 前缀树的根节点
    typename Node::ptr m_root;
};

// 构造函数
template <class T>
RadixTree<T>::RadixTree(){
    // 初始化 root 节点
    this->m_root = std::make_shared<Node>(Node::STATIC);
}

/**
 * @brief：将 key-value 对写入 radix tree
 * @param：key——键
 * @param：value——值
 */
template <class T>
void RadixTree<T>::put(const std::string& key, T value){
    // 倘若 key 对应节点已存在，则直接更新 value 即可
    typename Node::ptr target = this->get(key);
    if (target != nullptr && target->nodeType == Node::PARAM){
        target->value = value;
        return;
    }

    // 需要处理的路径
    std::string path = key;
    // 沿着根节点开始处理. move 为检索过程中途径的节点
    typename Node::ptr move = this->m_root;
    // 途径节点 priority 递增
    move->priority++;
    // 若是插入到 radix tree 中的首个 kv 对，直接写入 root 节点即可
    if (move->path.empty() && move->children.empty()){
        // 直接插入根节点即可
        move->insert(path,path,value);
        return;
    }

    while (true){
        // 找到当前节点和 key 的共同前缀的长度
        int longestPrefix = move->longestCommonPrefix(path);

        // 如果公共前缀长度小于当前节点下相对路径的长度，则需要对当前节点进行拆分
        if (longestPrefix < move->path.size()){
            /**
             * 构造一个新的子节点，承袭当前节点的信息
             * 1）nodeType：节点类型
             * 2）fullPath：完整路径
             * 3）value：存储值
             * 4）children：子节点列表
             * 5）indices：子节点首字母索引
             * 6）priority：优先级（途径的 key 数量）
             * 7）path：节点相对路径
             */
            typename Node::ptr child = std::make_shared<Node>(move->nodeType);
            child->fullPath = move->fullPath;
            child->value = move->value;
            child->indices = move->indices;
            child->children = move->children;
            // 由于途经 move 时已经先递增了 priority，所以此处需要进行扣减，因为 child 不被当前 key 途经
            child->priority = move->priority - 1;
            // 更新为除去公共前缀的后续部分
            child->path.assign(move->path.begin()+longestPrefix,move->path.end());

            /**
             * 对当前节点的信息进行更新
             */
            // 节点类型更新为 STATIC，刚进行过切分，不可能存在 value 挂载在当前节点下
            move->nodeType = Node::STATIC;
            // 完整路径与相对路径都需要截止到公共前缀的位置
            move->fullPath.assign(move->fullPath.begin(),move->fullPath.end()-(move->path.size() - longestPrefix));
            move->path.assign(move->path.begin(),move->path.begin()+longestPrefix);
            // 子节点列表与首字母索引更新
            move->children = {child};
            move->indices = {child->path[0]};
            // 存储值清除
            move->value = 0;
        }

        // 如果公共前缀长度小于 path 的长度，则对 path 进行拆分
        if (longestPrefix < path.size()){
            // 将 path 剔除公共前缀部分
            path.assign(path.begin()+longestPrefix,path.end());
            // 根据剔除公共前缀的后继部分首字母，进行检索
            char c = path[0];
            // 检索当前节点的子节点列表是否有首字母和 c 一致的子节点
            bool findChild = false;
            for (int i = 0; i < move->indices.size(); i++){
                // 找到首字母与 c 一致的子节点
                if (move->indices[i] == c){
                    // 接下来需要移动到 index i 对应的子节点，需要递增其 priority，并且调整其在 children 中的位置
                    int pos = move->incrementChildPrio(i);
                    // 移动到子节点的位置，开始下一轮检索
                    move = move->children[pos];
                    findChild = true;
                    break;
                }
            }

            if (findChild){
                continue;
            }

            // 已经不存在与 path 有公共前缀的子节点了，则将 path 构造成子节点插入到 children 中
            // 1）构造子节点
            typename Node::ptr child = std::make_shared<Node>(Node::PARAM);
            child->insert(path,key,value);

            // 2）插入子节点列表
            move->indices += c;
            move->addChild(child);

            // 3）递增子节点的 priority
            move->incrementChildPrio(move->indices.size()-1);
            return;
        }

        // 当前节点相对路径恰好和 path 相等，则直接插入当前节点
        move->insert(path,key,value);
        return;
    }
}

/**
 * @brief：查询 key 对应 value
 * @param：key——键
 * @param：receiver——接收 value 的容器
 * @return：true——数据存在 false——数据不存在
 */
template <class T>
bool RadixTree<T>::get(const std::string& key, T& receiver){
    typename Node::ptr target = this->get(key);
    if (target == nullptr || target->nodeType != Node::PARAM){
        return false;
    }

    receiver = target->value;
    return true;
}

/**
 * 获取 key 对应的 radix node
 */
template <class T>
typename RadixTree<T>::Node::ptr RadixTree<T>::get(const std::string& key){
    std::string path = key;
    typename Node::ptr move = this->m_root;
    while (true){
        // 找到 path 和当前节点相对路径的公共前缀长度
        int longestPrefix = move->longestCommonPrefix(path);
        // 如果公共前缀小于当前节点相对路径的长度，则说明目标节点不存在
        if (longestPrefix < move->path.size()){
            return nullptr;
        }

        // 当前节点的相对路径和 path 完全相等，返回当前节点
        if (longestPrefix == path.size()){
            return move;
        }

        // 剔除公共前缀部分，剩余部分继续检索处理
        path.assign(path.begin()+longestPrefix,path.end());
        char c = path[0];
        bool findChild = false;
        for (int i = 0; i < move->indices.size(); i++){
            if (move->indices[i] == c){
                move = move->children[i];
                findChild = true;
                break;
            }
        }

        if (!findChild){
            return nullptr;
        }
    }   
}

// 构造函数
template <class T>
RadixTree<T>::Node::Node(Type typ):nodeType(typ),path(""),fullPath(""),value(0){}

/**
 * @brief 向节点中插入 value 值，该方法会将节点类型更新为 PARAM
 * @param：path——节点相对路径
 * @param：fullPath——节点完整路径
 * @param：value——存储值
 */
template <class T>
void RadixTree<T>::Node::insert(const std::string& path, const std::string& fullPath, T value){
    this->path = path;
    this->fullPath = fullPath;
    this->value = value;
    this->nodeType = Node::PARAM;
}

/** 
 * 获取 path 和当前节点相对路径—— node.path 的公共前缀的长度
*/
template <class T>
int RadixTree<T>::Node::longestCommonPrefix(const std::string& path) const{
    int i = 0;
    int end = std::min(this->path.size(), path.size());
    while (i < end && path[i] == this->path[i]){
        i++;
    }
    return i;
}

/**
 * 1）增加 children[pos] 子节点的 priority
 * 2）调整 children 中子节点的顺序，保证按照 priority 从高到底进行排序
 * 3）若 children 顺序发生了变化，则对 indices 执行相应的调整
 * 4）返回 children[pos] 在调整顺序后新的 index
 */
template <class T>
int RadixTree<T>::Node::incrementChildPrio(const int pos){
    this->children[pos]->priority++;
    // child[pos] 调整顺序后的新位置
    int newPos = pos;

    // 保证 children 按照 priority 由高到低的顺序进行排列
    while (newPos > 0 && this->children[newPos-1]->priority < this->children[newPos]->priority){
        std::swap(this->children[newPos-1],this->children[newPos]);
        std::swap(this->indices[newPos-1],this->indices[newPos]);
        newPos--;
    }

    return newPos;
}

// 追加一个新的子节点
template <class T>
void RadixTree<T>::Node::addChild(Node::ptr child){
    this->children.push_back(child);
}
}}