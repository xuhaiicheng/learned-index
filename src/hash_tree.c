#include "../inc/hash_tree.h"

Hash_Tree_Root *hash_tree_create(int left, int right, int b_tree_num) {
    Hash_Tree_Root *root = (Hash_Tree_Root *)malloc(sizeof(Hash_Tree_Root));
    root->left = left, root->right = right;
    root->b_tree_num = b_tree_num;
    root->b_tree_node =
        (struct B_Tree **)malloc(b_tree_num * sizeof(struct B_Tree *));
    for (int i = 0; i < b_tree_num; i++) {
        root->b_tree_node[i] = b_tree_create();
    }
    return root;
}

struct B_Tree *hash_find_b_tree(const Hash_Tree_Root *root, int key) {
    int mod = root->b_tree_num;
    int b_tree_index = ((key % mod) + mod) % mod;
    return root->b_tree_node[b_tree_index];
}

void hash_tree_free(Hash_Tree_Root *root) {
    for (int i = 0; i < root->b_tree_num; i++) {
        b_tree_free(root->b_tree_node[i]);
    }
    free(root->b_tree_node);
    free(root);
    root = NULL;
}

bool hash_tree_exist(const Hash_Tree_Root *root, int key) {
    return b_tree_exist(hash_find_b_tree(root, key), key);
}

void hash_tree_erase(const Hash_Tree_Root *root, int key) {
    b_tree_erase(hash_find_b_tree(root, key), key);
}

void hash_tree_insert(const Hash_Tree_Root *root, int key, const char *s) {
    b_tree_insert(hash_find_b_tree(root, key), key, s);
}

KV_Node *hash_tree_query(const Hash_Tree_Root *root, int key) {
    return b_tree_query(hash_find_b_tree(root, key), key);
}

void print_hash_tree_node(const Hash_Tree_Root *root, int key) {
    print_kv_node(hash_find_b_tree(root, key));
}