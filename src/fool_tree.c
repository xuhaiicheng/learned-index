#include "../inc/fool_tree.h"

Fool_Tree_Root* fool_tree_create(int left, int right, int b_tree_num){
    assert(right > left);
    Fool_Tree_Root* root = (Fool_Tree_Root*)malloc(sizeof(Fool_Tree_Root));
    root->left = left, root->right = right;
    root->b_tree_num = b_tree_num;
    long long range_sum = (long long)right - (long long)left;
    root->range_num = range_sum / b_tree_num;// 每个B树负责的key值范围
    root->b_tree_node = (struct B_Tree**)malloc(b_tree_num * sizeof(struct B_Tree*));
    for(int i = 0; i < b_tree_num; i ++){
        root->b_tree_node[i] = b_tree_create();
    }
    return root;
}

struct B_Tree* fool_find_b_tree(const Fool_Tree_Root* root, int key){
    int b_tree_index = ((long long)key - root->left) / root->range_num;
    if(b_tree_index < 0) b_tree_index = 0;
    if(b_tree_index >= root->b_tree_num) b_tree_index = root->b_tree_num - 1;
    return root->b_tree_node[b_tree_index];
}

void fool_tree_free(Fool_Tree_Root* root){
    for(int i = 0; i < root->b_tree_num; i ++){
        b_tree_free(root->b_tree_node[i]);
    }
    free(root->b_tree_node);
    free(root);
    root = NULL;
}

bool fool_tree_exist(const Fool_Tree_Root* root, int key){
    return b_tree_exist(fool_find_b_tree(root, key), key);
}

void fool_tree_erase(const Fool_Tree_Root* root, int key){
    b_tree_erase(fool_find_b_tree(root, key), key);
}

void fool_tree_insert(const Fool_Tree_Root* root, int key, const char* s){
    b_tree_insert(fool_find_b_tree(root, key), key, s);
}

KV_Node* fool_tree_query(const Fool_Tree_Root* root, int key){
    return b_tree_query(fool_find_b_tree(root, key), key);
}

void print_fool_tree_node(const Fool_Tree_Root* root, int key){
    print_kv_node(fool_find_b_tree(root, key));
}