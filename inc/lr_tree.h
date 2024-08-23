#ifndef LR_TREE_H_
#define LR_TREE_H_
#include "b_tree.h"
#include "utility.h"
// ---------------------宏定义--------------------
#define MAX_BRANCH 100 // 线性回归树的分支的最大数量
#define MAX_LAYER 3    // 线性回归树的最高层数

// --------------------结构体定义------------------
// 线性回归树的叶子节点
typedef struct LR_Tree_Leaf {
    int left, right; // 该叶子节点负责的key值的范围[left, right]
    double k, b;     // 拟合直线的斜率和截距
    int b_tree_num;  // 该叶子节点下B树数量

    struct B_Tree **b_tree_node; // B树子节点指针数组
} LR_Tree_Leaf;

// 线性回归树的根节点
typedef struct LR_Tree_Root {
    int leaf_num;        // 叶子节点的数量
    int left, right;     // 该节点负责的key值的范围[left, right]
    int *right_endpoint; // 按概率均分之后每一段的右端点

    LR_Tree_Leaf **leaf_node; // 叶子节点指针数组
} LR_Tree_Root;
// ---------------------函数原型-------------------
// 基于正态分布特征创建一个线性回归树, 并返回其根节点指针
LR_Tree_Root *lr_tree_create(double mean, double sigma, int branch,
                             int b_tree_num, int left, int right);
// 对划归的每一段进行线性拟合, 创建叶子节点并分配若干B树子节点
LR_Tree_Leaf *lr_tree_leaf_create(double mean, double sigma, int b_tree_num,
                                  int left, int right);
// 基于key值找到分治该key值的那个B树并返回其指针
struct B_Tree *find_b_tree(const LR_Tree_Root *root, int key);
// 释放线性回归树的内存
void lr_tree_free(LR_Tree_Root *root);
// 判断线性回归树中是否存储了指定key值的元素
bool lr_tree_exist(const LR_Tree_Root *lr_tree, int key);
// 删除线性回归树中键值为key的元素(如果有)
void lr_tree_erase(const LR_Tree_Root *lr_tree, int key);
// 向线性回归树中插入(或者更新)键值为key, value值为str字符串的元素
void lr_tree_insert(const LR_Tree_Root *lr_tree, int key, const char *s);
// 返回键值key对应的线性回归树元素, 若是无则返回NULL
KV_Node *lr_tree_query(const LR_Tree_Root *lr_tree, int key);
// 打印线性回归树中节点的信息
void print_lr_tree_root(const LR_Tree_Root *lr_tree, int key);

#endif // LR_TREE_H_