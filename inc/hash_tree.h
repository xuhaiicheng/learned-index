#ifndef HASH_TREE_H_
#define HASH_TREE_H_
#include "b_tree.h"
#include "utility.h"
// --------------------结构体定义------------------
// hash tree的根节点
typedef struct Hash_Tree_Root{
    int left, right;// 负责的key值的范围[left, right]
    int b_tree_num;// 内部的B树数量

    struct B_Tree** b_tree_node;// B树子节点指针数组
}Hash_Tree_Root;
// ---------------------函数原型-------------------
// 创建一颗hash tree， 返回其根节点指针
Hash_Tree_Root* hash_tree_create(int left, int right, int b_tree_num);
// 查找分管当前key值的是哪一颗B树并返回其节点指针
struct B_Tree* hash_find_b_tree(const Hash_Tree_Root* root, int key);
// 释放hash tree的内存
void hash_tree_free(Hash_Tree_Root* root);
// 判断hash tree中是否存储了指定key值的元素
bool hash_tree_exist(const Hash_Tree_Root* root, int key);
// 删除hash tree中键值为key的元素(如果有)
void hash_tree_erase(const Hash_Tree_Root* root, int key);
// 向hash tree中插入(或者更新)键值为key, value值为str字符串的元素
void hash_tree_insert(const Hash_Tree_Root* root, int key, const char* s);
// 返回键值key对应的hash tree元素, 若是无则返回NULL
KV_Node* hash_tree_query(const Hash_Tree_Root* root, int key);
// 打印hash tree中节点的信息
void print_hash_tree_node(const Hash_Tree_Root* root, int key);

#endif // HASH_TREE_H_