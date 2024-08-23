#include "../inc/lr_tree.h"

LR_Tree_Root *lr_tree_create(double mean, double sigma, int branch,
                             int b_tree_num, int left, int right) {
    // 本次递归创建的线性回归树节点
    LR_Tree_Root *root = (LR_Tree_Root *)malloc(sizeof(LR_Tree_Root));
    root->left = left, root->right = right; // key值左右范围
    root->leaf_num = branch; // 根节点下连接多少叶子节点
    // 记录每一个叶子节点的指针
    root->leaf_node = (LR_Tree_Leaf **)malloc(branch * sizeof(LR_Tree_Leaf *));
    // 基于正态分布的累积分布函数求出按概率均分之后每一段的右端点
    root->right_endpoint = (int *)malloc(branch * sizeof(int));
    for (int i = 0; i < branch; i++) {
        double cdf_val = ((double)i + 1.0) / (double)branch;
        long long x;
        if (1.0 - cdf_val > EPSILON)
            x = normal_icdf(mean, sigma, cdf_val);
        else
            x = INT_MAX - 1;
        if (x >= INT_MAX)
            root->right_endpoint[i] = INT_MAX - 1;
        else if (x <= INT_MIN)
            root->right_endpoint[i] = INT_MIN + 1;
        else
            root->right_endpoint[i] = (int)x;
    }
    root->right_endpoint[branch - 1] = INT_MAX - 1; // 最后一个分段进行兜底
    for (int i = 0; i < branch; i++) {
        // 为根节点分配出若干叶子节点
        if (i) {
            // 确保每个区间都有至少一个数
            assert(root->right_endpoint[i] > root->right_endpoint[i - 1]);
        }
        int leaf_left =
            (i == 0) ? (INT_MIN + 1) : (root->right_endpoint[i - 1] + 1);
        int leaf_right = root->right_endpoint[i];
        // printf("[%d, %d]\n", leaf_left, leaf_right);
        root->leaf_node[i] =
            lr_tree_leaf_create(mean, sigma, b_tree_num, leaf_left, leaf_right);
    }
    return root;
}

LR_Tree_Leaf *lr_tree_leaf_create(double mean, double sigma, int b_tree_num,
                                  int left, int right) {
    LR_Tree_Leaf *leaf = (LR_Tree_Leaf *)malloc(sizeof(LR_Tree_Leaf));
    leaf->left = left, leaf->right = right;
    leaf->b_tree_num = b_tree_num;
    // 基于最小二乘给出拟合[left, right]段的直线参数
    linear_fitting(mean, sigma, left, right, &leaf->k, &leaf->b, b_tree_num);
    // 此时使用y = k * x + b拟合正态分布函数CDF的x = [left, right]段
    // 而y值落在[0, b_tree_num - 1]之上
    leaf->b_tree_node = (struct B_Tree**)malloc(b_tree_num * sizeof(struct B_Tree*));
    for(int i = 0; i < b_tree_num; i ++){
        // 为该叶子节点赋予b_tree_num个B树子节点
        leaf->b_tree_node[i] = b_tree_create();
    }
    return leaf;
}

struct B_Tree *find_b_tree(const LR_Tree_Root *root, int key){
    // 基于二分选中对应的叶子节点分支
    int*arr = root->right_endpoint;
    int n = root->leaf_num;// 叶子的数量
    int l = 0, r = n - 1;
    // 通过二分找到第一个存储大于等于key值的right_endpoint数组值的索引
    while(l < r){
        int mid = (l + r) >> 1;
        if(arr[mid] >= key){
            r = mid;
        }else{
            l = mid + 1;
        }
    }
    LR_Tree_Leaf* leaf = root->leaf_node[l];// 取出二分到的叶子节点指针
    // 根据拟合公式计算出是哪一个B树
    int b_tree_index = (int)(leaf->k * key + leaf->b);
    if(b_tree_index >= leaf->b_tree_num) b_tree_index = leaf->b_tree_num - 1;
    else if(b_tree_index < 0) b_tree_index = 0;
    return leaf->b_tree_node[b_tree_index];
}

void lr_tree_free(LR_Tree_Root *root){
    int n = root->leaf_num;
    for(int i = 0; i < n; i ++){
        LR_Tree_Leaf* leaf = root->leaf_node[i];
        int m = leaf->b_tree_num;
        for(int j = 0; j < m; j ++){
            // 释放每个叶子的所有B树内存
            b_tree_free(leaf->b_tree_node[j]);
        }
        leaf->b_tree_num = 0;
        free(leaf->b_tree_node);
        leaf->b_tree_node = NULL;
    }
    root->leaf_num = 0;
    free(root->right_endpoint);
    free(root->leaf_node);
    root->right_endpoint = NULL;
    root->leaf_node = NULL;
    free(root);
    root = NULL;
}

bool lr_tree_exist(const LR_Tree_Root *lr_tree, int key){
    return b_tree_exist(find_b_tree(lr_tree, key), key);
}

void lr_tree_erase(const LR_Tree_Root *lr_tree, int key){
    b_tree_erase(find_b_tree(lr_tree, key), key);
}

void lr_tree_insert(const LR_Tree_Root *lr_tree, int key, const char *s){
    b_tree_insert(find_b_tree(lr_tree, key), key, s);
}

KV_Node *lr_tree_query(const LR_Tree_Root *lr_tree, int key){
    return b_tree_query(find_b_tree(lr_tree, key), key);
}

void print_lr_tree_root(const LR_Tree_Root *lr_tree, int key){
    print_kv_node(lr_tree_query(lr_tree, key));
}