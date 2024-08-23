#ifndef FOOL_TREE_H_
#define FOOL_TREE_H_
#include "b_tree.h"
#include "utility.h"
// --------------------结构体定义------------------
// fool树的根节点
typedef struct Fool_Tree_Root{
    int left, right;// 负责的key值的范围[left, right]
    int b_tree_num;// 内部的B树数量
    long long range_num;// 每个B树负责的key的数量

    struct B_Tree** b_tree_node;// B树子节点指针数组
}Fool_Tree_Root;
// ---------------------函数原型-------------------
// 创建一颗fool tree， 返回其根节点指针
Fool_Tree_Root* fool_tree_create(int left, int right, int b_tree_num);
// 查找分管当前key值的是哪一颗B树并返回其节点指针
struct B_Tree* fool_find_b_tree(const Fool_Tree_Root* root, int key);
// 释放fool tree的内存
void fool_tree_free(Fool_Tree_Root* root);
// 判断fool tree中是否存储了指定key值的元素
bool fool_tree_exist(const Fool_Tree_Root* root, int key);
// 删除fool tree中键值为key的元素(如果有)
void fool_tree_erase(const Fool_Tree_Root* root, int key);
// 向fool tree中插入(或者更新)键值为key, value值为str字符串的元素
void fool_tree_insert(const Fool_Tree_Root* root, int key, const char* s);
// 返回键值key对应的fool tree元素, 若是无则返回NULL
KV_Node* fool_tree_query(const Fool_Tree_Root* root, int key);
// 打印fool tree中节点的信息
void print_fool_tree_node(const Fool_Tree_Root* root, int key);

#endif // FOOL_TREE_H_