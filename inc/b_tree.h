// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#ifndef B_Tree_H
#define B_Tree_H

#include <stddef.h>
#include <stdint.h>
#include "utility.h"

struct B_Tree;

// B_Tree_new returns a new B-tree.
//
// Param elsize is the size of each element in the tree. Every element that
// is inserted, deleted, or searched will be this size.
// Param max_items is the maximum number of items per node. Setting this to
// zero will default to 256.
// Param compare is a function that compares items in the tree. See the 
// qsort stdlib function for an example of how this function works.
// Param udata is user-defined data that is passed to the compare callback
// and the item callbacks defined in B_Tree_set_item_callbacks.
//
// The B_Tree must be freed with B_Tree_free(). 
struct B_Tree *B_Tree_new(size_t elsize, size_t max_items,
    int (*compare)(const void *a, const void *b, void *udata),
    void *udata);

// B_Tree_new_with_allocator returns a new B_Tree using a custom allocator.
//
// See B_Tree_new for more information
struct B_Tree *B_Tree_new_with_allocator(
    void *(*malloc)(size_t), 
    void *(*realloc)(void *, size_t), 
    void (*free)(void*),
    size_t elsize, 
    size_t max_items,
    int (*compare)(const void *a, const void *b, void *udata),
    void *udata);

// B_Tree_set_item_callbacks sets the item clone and free callbacks that will be
// called internally by the B_Tree when items are inserted and removed.
//
// These callbacks are optional but may be needed by programs that require
// copy-on-write support by using the B_Tree_clone function.
//
// The clone function should return true if the clone succeeded or false if the
// system is out of memory.
void B_Tree_set_item_callbacks(struct B_Tree *B_Tree,
    bool (*clone)(const void *item, void *into, void *udata), 
    void (*free)(const void *item, void *udata));

// B_Tree_free removes all items from the B_Tree and frees any allocated memory.
void B_Tree_free(struct B_Tree *B_Tree);

// B_Tree_free removes all items from the B_Tree.
void B_Tree_clear(struct B_Tree *B_Tree);

// B_Tree_oom returns true if the last write operation failed because the system
// has no more memory available.
// 
// Functions that have the first param being a non-const B_Tree receiver are 
// candidates for possible out-of-memory conditions, such as B_Tree_set, 
// B_Tree_delete, B_Tree_load, etc.
bool B_Tree_oom(const struct B_Tree *B_Tree);

// B_Tree_height returns the height of the B_Tree from root to leaf or zero if
// the B_Tree is empty.
size_t B_Tree_height(const struct B_Tree *B_Tree);

// B_Tree_count returns the number of items in the B_Tree.
size_t B_Tree_count(const struct B_Tree *B_Tree);

// B_Tree_clone makes an instant copy of the B_Tree.
// This operation uses shadowing / copy-on-write.
struct B_Tree *B_Tree_clone(struct B_Tree *B_Tree);

// B_Tree_set inserts or replaces an item in the B_Tree. If an item is replaced
// then it is returned otherwise NULL is returned. 
//
// If the system fails allocate the memory needed then NULL is returned 
// and B_Tree_oom() returns true.
const void *B_Tree_set(struct B_Tree *B_Tree, const void *item);

// B_Tree_delete removes an item from the B-tree and returns it.
//
// Returns NULL if item not found.
// This operation may trigger node copies if the B_Tree was cloned using
// B_Tree_clone.
// If the system fails allocate the memory needed then NULL is returned 
// and B_Tree_oom() returns true.
const void *B_Tree_delete(struct B_Tree *B_Tree, const void *key);

// B_Tree_load is the same as B_Tree_set but is optimized for sequential bulk 
// loading. It can be up to 10x faster than B_Tree_set when the items are
// in exact order, but up to 25% slower when not in exact order.
//
// If the system fails allocate the memory needed then NULL is returned 
// and B_Tree_oom() returns true.
const void *B_Tree_load(struct B_Tree *B_Tree, const void *item);

// B_Tree_pop_min removes the first item in the B_Tree and returns it.
//
// Returns NULL if B_Tree is empty.
// This operation may trigger node copies if the B_Tree was cloned using
// B_Tree_clone.
// If the system fails allocate the memory needed then NULL is returned 
// and B_Tree_oom() returns true.
const void *B_Tree_pop_min(struct B_Tree *B_Tree);

// B_Tree_pop_min removes the last item in the B_Tree and returns it.
//
// Returns NULL if B_Tree is empty.
// This operation may trigger node copies if the B_Tree was cloned using
// B_Tree_clone.
// If the system fails allocate the memory needed then NULL is returned 
// and B_Tree_oom() returns true.
const void *B_Tree_pop_max(struct B_Tree *B_Tree);

// B_Tree_pop_min returns the first item in the B_Tree or NULL if B_Tree is empty.
const void *B_Tree_min(const struct B_Tree *B_Tree);

// B_Tree_pop_min returns the last item in the B_Tree or NULL if B_Tree is empty.
const void *B_Tree_max(const struct B_Tree *B_Tree);

// B_Tree_get returns the item based on the provided key. 
//
// Returns NULL if item is not found.
const void *B_Tree_get(const struct B_Tree *B_Tree, const void *key);

// B_Tree_ascend scans the tree within the range [pivot, last].
//
// In other words B_Tree_ascend iterates over all items that are 
// greater-than-or-equal-to pivot in ascending order.
//
// Param pivot can be NULL, which means all items are iterated over.
// Param iter can return false to stop iteration early.
// Returns false if the iteration has been stopped early.
bool B_Tree_ascend(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), void *udata);

// B_Tree_descend scans the tree within the range [pivot, first]. 

// In other words B_Tree_descend() iterates over all items that are 
// less-than-or-equal-to pivot in descending order.
//
// Param pivot can be NULL, which means all items are iterated over.
// Param iter can return false to stop iteration early.
// Returns false if the iteration has been stopped early.
bool B_Tree_descend(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), void *udata);

// B_Tree_set_hint is the same as B_Tree_set except that an optional "hint" can 
// be provided which may make the operation quicker when done as a batch or 
// in a userspace context.
const void *B_Tree_set_hint(struct B_Tree *B_Tree, const void *item,
    uint64_t *hint);

// B_Tree_get_hint is the same as B_Tree_get except that an optional "hint" can 
// be provided which may make the operation quicker when done as a batch or 
// in a userspace context.
const void *B_Tree_get_hint(const struct B_Tree *B_Tree, const void *key,
    uint64_t *hint);

// B_Tree_delete_hint is the same as B_Tree_delete except that an optional "hint"
// can be provided which may make the operation quicker when done as a batch or 
// in a userspace context.
const void *B_Tree_delete_hint(struct B_Tree *B_Tree, const void *key,
    uint64_t *hint);

// B_Tree_ascend_hint is the same as B_Tree_ascend except that an optional
// "hint" can be provided which may make the operation quicker when done as a
// batch or in a userspace context.
bool B_Tree_ascend_hint(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), 
    void *udata, uint64_t *hint);

// B_Tree_descend_hint is the same as B_Tree_descend except that an optional
// "hint" can be provided which may make the operation quicker when done as a
// batch or in a userspace context.
bool B_Tree_descend_hint(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), 
    void *udata, uint64_t *hint);

// B_Tree_set_searcher allows for setting a custom search function.
void B_Tree_set_searcher(struct B_Tree *B_Tree, 
    int (*searcher)(const void *items, size_t nitems, const void *key, 
        bool *found, void *udata));

// Loop-based iterator
struct B_Tree_iter *B_Tree_iter_new(const struct B_Tree *B_Tree);
void B_Tree_iter_free(struct B_Tree_iter *iter);
bool B_Tree_iter_first(struct B_Tree_iter *iter);
bool B_Tree_iter_last(struct B_Tree_iter *iter);
bool B_Tree_iter_next(struct B_Tree_iter *iter);
bool B_Tree_iter_prev(struct B_Tree_iter *iter);
bool B_Tree_iter_seek(struct B_Tree_iter *iter, const void *key);
const void *B_Tree_iter_item(struct B_Tree_iter *iter);


// DEPRECATED: use `B_Tree_new_with_allocator`
void B_Tree_set_allocator(void *(malloc)(size_t), void (*free)(void*));

// 创建一颗新的存储KV_Node元素的B树
struct B_Tree *b_tree_create();

// 判断B树中是否存储了指定key值的元素
bool b_tree_exist(const struct B_Tree *B_Tree, int key);

// 删除B树中键值为key值得元素(如果有)
void b_tree_erase(const struct B_Tree *B_Tree, int key);

// 向B树中插入(或者更新)键值为key, value值为str字符串的元素
void b_tree_insert(const struct B_Tree *B_Tree, int key, const char* s);

// 返回键值key对应的B树元素, 若是无则返回NULL
KV_Node* b_tree_query(const struct B_Tree *B_Tree, int key);

// 打印B树中节点的信息
void print_b_tree_node(const struct B_Tree *B_Tree, int key);

// 释放B树内存
void b_tree_free(struct B_Tree *B_Tree);
#endif