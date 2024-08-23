// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef B_Tree_STATIC
#include "../inc/b_tree.h"
#else
#define B_Tree_EXTERN static
#endif

#ifndef B_Tree_EXTERN
#define B_Tree_EXTERN
#endif

static void *(*_B_Tree_malloc)(size_t) = NULL;
static void (*_B_Tree_free)(void *) = NULL;

// DEPRECATED: use `B_Tree_new_with_allocator`
B_Tree_EXTERN
void B_Tree_set_allocator(void *(malloc)(size_t), void (*free)(void*)) {
    _B_Tree_malloc = malloc;
    _B_Tree_free = free;
}

enum B_Tree_delact {
    B_Tree_DELKEY, B_Tree_POPFRONT, B_Tree_POPBACK, B_Tree_POPMAX,
};

static size_t B_Tree_align_size(size_t size) {
    size_t boundary = sizeof(uintptr_t);
    return size < boundary ? boundary :
           size&(boundary-1) ? size+boundary-(size&(boundary-1)) : 
           size;
}

#ifdef B_Tree_NOATOMICS
typedef int B_Tree_rc_t;
static int B_Tree_rc_load(B_Tree_rc_t *ptr) {
    return *ptr;
}
static int B_Tree_rc_fetch_sub(B_Tree_rc_t *ptr, int val) {
    int rc = *ptr;
    *ptr -= val;
    return rc;
}
static int B_Tree_rc_fetch_add(B_Tree_rc_t *ptr, int val) {
    int rc = *ptr;
    *ptr += val;
    return rc;
}
#else 
#include <stdatomic.h>
typedef atomic_int B_Tree_rc_t;
static int B_Tree_rc_load(B_Tree_rc_t *ptr) {
    return atomic_load(ptr);
}
static int B_Tree_rc_fetch_sub(B_Tree_rc_t *ptr, int delta) {
    return atomic_fetch_sub(ptr, delta);
}
static int B_Tree_rc_fetch_add(B_Tree_rc_t *ptr, int delta) {
    return atomic_fetch_add(ptr, delta);
}
#endif

struct B_Tree_node {
    B_Tree_rc_t rc;
    bool leaf;
    size_t nitems:16;
    char *items;
    struct B_Tree_node *children[];
};

struct B_Tree {
    void *(*malloc)(size_t);
    void *(*realloc)(void *, size_t);
    void (*free)(void *);
    int (*compare)(const void *a, const void *b, void *udata);
    int (*searcher)(const void *items, size_t nitems, const void *key,
        bool *found, void *udata);
    bool (*item_clone)(const void *item, void *into, void *udata);
    void (*item_free)(const void *item, void *udata);
    void *udata;             // user data
    struct B_Tree_node *root; // root node or NULL if empty tree
    size_t count;            // number of items in tree
    size_t height;           // height of tree from root to leaf
    size_t max_items;        // max items allowed per node before needing split
    size_t min_items;        // min items allowed per node before needing join
    size_t elsize;           // size of user item
    bool oom;                // last write operation failed due to no memory
    size_t spare_elsize;     // size of each spare element. This is aligned
    char spare_data[];       // spare element spaces for various operations
};

static void *B_Tree_spare_at(const struct B_Tree *B_Tree, size_t index) {
    return (void*)(B_Tree->spare_data+B_Tree->spare_elsize*index);
}

B_Tree_EXTERN
void B_Tree_set_searcher(struct B_Tree *B_Tree, 
    int (*searcher)(const void *items, size_t nitems, const void *key, 
        bool *found, void *udata))
{
    B_Tree->searcher = searcher;
}

#define B_Tree_NSPARES 4
#define B_Tree_SPARE_RETURN B_Tree_spare_at(B_Tree, 0) // returned values
#define B_Tree_SPARE_NODE   B_Tree_spare_at(B_Tree, 1) // clone in B_Tree_node_copy
#define B_Tree_SPARE_POPMAX B_Tree_spare_at(B_Tree, 2) // B_Tree_delete popmax
#define B_Tree_SPARE_CLONE  B_Tree_spare_at(B_Tree, 3) // cloned inputs 

static void *B_Tree_get_item_at(struct B_Tree *B_Tree, struct B_Tree_node *node, 
    size_t index)
{
    return node->items+B_Tree->elsize*index;
}

static void B_Tree_set_item_at(struct B_Tree *B_Tree, struct B_Tree_node *node,
    size_t index, const void *item) 
{
    void *slot = B_Tree_get_item_at(B_Tree, node, index);
    memcpy(slot, item, B_Tree->elsize);
}

static void B_Tree_swap_item_at(struct B_Tree *B_Tree, struct B_Tree_node *node,
    size_t index, const void *item, void *into)
{ 
    void *ptr = B_Tree_get_item_at(B_Tree, node, index);
    memcpy(into, ptr, B_Tree->elsize);
    memcpy(ptr, item, B_Tree->elsize);
}

static void B_Tree_copy_item_into(struct B_Tree *B_Tree, 
    struct B_Tree_node *node, size_t index, void *into)
{ 
    memcpy(into, B_Tree_get_item_at(B_Tree, node, index), B_Tree->elsize);
}

static void B_Tree_node_shift_right(struct B_Tree *B_Tree, struct B_Tree_node *node,
    size_t index)
{
    size_t num_items_to_shift = node->nitems - index;
    memmove(node->items+B_Tree->elsize*(index+1), 
        node->items+B_Tree->elsize*index, num_items_to_shift*B_Tree->elsize);
    if (!node->leaf) {
        memmove(&node->children[index+1], &node->children[index],
            (num_items_to_shift+1)*sizeof(struct B_Tree_node*));
    }
    node->nitems++;
}

static void B_Tree_node_shift_left(struct B_Tree *B_Tree, struct B_Tree_node *node,
    size_t index, bool for_merge) 
{
    size_t num_items_to_shift = node->nitems - index - 1;
    memmove(node->items+B_Tree->elsize*index, 
        node->items+B_Tree->elsize*(index+1), num_items_to_shift*B_Tree->elsize);
    if (!node->leaf) {
        if (for_merge) {
            index++;
            num_items_to_shift--;
        }
        memmove(&node->children[index], &node->children[index+1],
            (num_items_to_shift+1)*sizeof(struct B_Tree_node*));
    }
    node->nitems--;
}

static void B_Tree_copy_item(struct B_Tree *B_Tree, struct B_Tree_node *node_a,
    size_t index_a, struct B_Tree_node *node_b, size_t index_b) 
{
    memcpy(B_Tree_get_item_at(B_Tree, node_a, index_a), 
        B_Tree_get_item_at(B_Tree, node_b, index_b), B_Tree->elsize);
}

static void B_Tree_node_join(struct B_Tree *B_Tree, struct B_Tree_node *left,
    struct B_Tree_node *right)
{
    memcpy(left->items+B_Tree->elsize*left->nitems, right->items,
        right->nitems*B_Tree->elsize);
    if (!left->leaf) {
        memcpy(&left->children[left->nitems], &right->children[0],
            (right->nitems+1)*sizeof(struct B_Tree_node*));
    }
    left->nitems += right->nitems;
}

static int _B_Tree_compare(const struct B_Tree *B_Tree, const void *a, 
    const void *b)
{
    return B_Tree->compare(a, b, B_Tree->udata);
}

static size_t B_Tree_node_bsearch(const struct B_Tree *B_Tree,
    struct B_Tree_node *node, const void *key, bool *found) 
{
    size_t i = 0;
    size_t n = node->nitems;
    while ( i < n ) {
        size_t j = (i + n) >> 1;
        void *item = B_Tree_get_item_at((void*)B_Tree, node, j);
        int cmp = _B_Tree_compare(B_Tree, key, item);
        if (cmp == 0) {
            *found = true;
            return j;
        } else if (cmp < 0) {
            n = j;
        } else {
            i = j+1;
        }
    }
    *found = false;
    return i;
}

static int B_Tree_node_bsearch_hint(const struct B_Tree *B_Tree,
    struct B_Tree_node *node, const void *key, bool *found, uint64_t *hint,
    int depth) 
{
    int low = 0;
    int high = node->nitems-1;
    if (hint && depth < 8) {
        size_t index = (size_t)((uint8_t*)hint)[depth];
        if (index > 0) {
            if (index > (size_t)(node->nitems-1)) {
                index = node->nitems-1;
            }
            void *item = B_Tree_get_item_at((void*)B_Tree, node, (size_t)index);
            int cmp = _B_Tree_compare(B_Tree, key, item);
            if (cmp == 0) {
                *found = true;
                return index;
            }
            if (cmp > 0) {
                low = index+1;
            } else {
                high = index-1;
            }
        }
    }
    int index;
    while ( low <= high ) {
        int mid = (low + high) / 2;
        void *item = B_Tree_get_item_at((void*)B_Tree, node, (size_t)mid);
        int cmp = _B_Tree_compare(B_Tree, key, item);
        if (cmp == 0) {
            *found = true;
            index = mid;
            goto done;
        }
        if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    *found = false;
    index = low;
done:
    if (hint && depth < 8) {
        ((uint8_t*)hint)[depth] = (uint8_t)index;
    }
    return index;
}

static size_t B_Tree_memsize(size_t elsize, size_t *spare_elsize) {
    size_t size = B_Tree_align_size(sizeof(struct B_Tree));
    size_t elsize_aligned = B_Tree_align_size(elsize);
    size += elsize_aligned * B_Tree_NSPARES;
    if (spare_elsize) *spare_elsize = elsize_aligned;
    return size;
}

B_Tree_EXTERN
struct B_Tree *B_Tree_new_with_allocator(
    void *(*malloc_)(size_t), 
    void *(*realloc_)(void *, size_t), 
    void (*free_)(void*),
    size_t elsize, 
    size_t max_items,
    int (*compare)(const void *a, const void *b, void *udata),
    void *udata)
{
    (void)realloc_; // realloc not used
    malloc_ = malloc_ ? malloc_ : _B_Tree_malloc ? _B_Tree_malloc : malloc;
    free_ = free_ ? free_ : _B_Tree_free ? _B_Tree_free : free;
    
    // normalize max_items
    size_t spare_elsize;
    size_t size = B_Tree_memsize(elsize, &spare_elsize);
    struct B_Tree *B_Tree = malloc_(size);
    if (!B_Tree) {
        return NULL;
    }
    memset(B_Tree, 0, size);
    size_t deg = max_items/2;
    deg = deg == 0 ? 128 : deg == 1 ? 2 : deg;
    B_Tree->max_items = deg*2 - 1; // max items per node. max children is +1
    if (B_Tree->max_items > 2045) {
        // there must be a reasonable limit.
        B_Tree->max_items = 2045;
    }
    B_Tree->min_items = B_Tree->max_items / 2;    
    B_Tree->compare = compare;
    B_Tree->elsize = elsize;
    B_Tree->udata = udata;
    B_Tree->malloc = malloc_;
    B_Tree->free = free_;
    B_Tree->spare_elsize = spare_elsize;
    return B_Tree;
}

B_Tree_EXTERN
struct B_Tree *B_Tree_new(size_t elsize, size_t max_items,
    int (*compare)(const void *a, const void *b, void *udata), void *udata)
{
    return B_Tree_new_with_allocator(NULL, NULL, NULL, elsize, max_items,
        compare, udata);
}

static size_t B_Tree_node_size(struct B_Tree *B_Tree, bool leaf,
    size_t *items_offset)
{
    size_t size = sizeof(struct B_Tree_node);
    if (!leaf) {
        // add children as flexible array
        size += sizeof(struct B_Tree_node*)*(B_Tree->max_items+1);
    }
    if (items_offset) *items_offset = size;
    size += B_Tree->elsize*B_Tree->max_items;
    size = B_Tree_align_size(size);
    return size;
}

static struct B_Tree_node *B_Tree_node_new(struct B_Tree *B_Tree, bool leaf) {
    size_t items_offset;
    size_t size = B_Tree_node_size(B_Tree, leaf, &items_offset);
    struct B_Tree_node *node = B_Tree->malloc(size);
    if (!node) {
        return NULL;
    }
    memset(node, 0, size);
    node->leaf = leaf;
    node->items = (char*)node+items_offset;
    return node;
}

static void B_Tree_node_free(struct B_Tree *B_Tree, struct B_Tree_node *node) {
    if (B_Tree_rc_fetch_sub(&node->rc, 1) > 0) {
        return;
    }
    if (!node->leaf) {
        for (size_t i = 0; i < (size_t)(node->nitems+1); i++) {
            B_Tree_node_free(B_Tree, node->children[i]);
        }
    }
    if (B_Tree->item_free) {
        for (size_t i = 0; i < node->nitems; i++) {
            void *item = B_Tree_get_item_at(B_Tree, node, i);
            B_Tree->item_free(item, B_Tree->udata);
        }
    }
    B_Tree->free(node);
}

static struct B_Tree_node *B_Tree_node_copy(struct B_Tree *B_Tree,
    struct B_Tree_node *node)
{
    struct B_Tree_node *node2 = B_Tree_node_new(B_Tree, node->leaf);
    if (!node2) {
        return NULL;
    }
    node2->nitems = node->nitems;
    size_t items_cloned = 0;
    if (!node2->leaf) {
        for (size_t i = 0; i < (size_t)(node2->nitems+1); i++) {
            node2->children[i] = node->children[i];
            B_Tree_rc_fetch_add(&node2->children[i]->rc, 1);
        }
    }
    if (B_Tree->item_clone) {
        for (size_t i = 0; i < node2->nitems; i++) {
            void *item = B_Tree_get_item_at(B_Tree, node, i);
            if (!B_Tree->item_clone(item, B_Tree_SPARE_NODE, B_Tree->udata)) {
                goto failed;
            }
            B_Tree_set_item_at(B_Tree, node2, i, B_Tree_SPARE_NODE);
            items_cloned++;
        }
    } else {
        for (size_t i = 0; i < node2->nitems; i++) {
            void *item = B_Tree_get_item_at(B_Tree, node, i);
            B_Tree_set_item_at(B_Tree, node2, i, item);
        }
    }
    return node2;
failed:
    if (!node2->leaf) {
        for (size_t i = 0; i < (size_t)(node2->nitems+1); i++) {
            B_Tree_rc_fetch_sub(&node2->children[i]->rc, 1);
        }
    }
    if (B_Tree->item_free) {
        for (size_t i = 0; i < items_cloned; i++) {
            void *item = B_Tree_get_item_at(B_Tree, node2, i);
            B_Tree->item_free(item, B_Tree->udata);
        }
    }
    B_Tree->free(node2);
    return NULL;
}

#define B_Tree_cow_node_or(bnode, code) { \
    if (B_Tree_rc_load(&(bnode)->rc) > 0) { \
        struct B_Tree_node *node2 = B_Tree_node_copy(B_Tree, (bnode)); \
        if (!node2) { code; } \
        B_Tree_node_free(B_Tree, bnode); \
        (bnode) = node2; \
    } \
}

B_Tree_EXTERN
void B_Tree_clear(struct B_Tree *B_Tree) {
    if (B_Tree->root) {
        B_Tree_node_free(B_Tree, B_Tree->root);
    }
    B_Tree->oom = false;
    B_Tree->root = NULL;
    B_Tree->count = 0;
    B_Tree->height = 0;
}

B_Tree_EXTERN
void B_Tree_free(struct B_Tree *B_Tree) {
    B_Tree_clear(B_Tree);
    B_Tree->free(B_Tree);
}

B_Tree_EXTERN
void B_Tree_set_item_callbacks(struct B_Tree *B_Tree,
    bool (*clone)(const void *item, void *into, void *udata), 
    void (*free)(const void *item, void *udata))
{
    B_Tree->item_clone = clone;
    B_Tree->item_free = free;
}

B_Tree_EXTERN
struct B_Tree *B_Tree_clone(struct B_Tree *B_Tree) {
    if (!B_Tree) {
        return NULL;
    }
    size_t size = B_Tree_memsize(B_Tree->elsize, NULL);
    struct B_Tree *B_Tree2 = B_Tree->malloc(size);
    if (!B_Tree2) {
        return NULL;
    }
    memcpy(B_Tree2, B_Tree, size);
    if (B_Tree2->root) B_Tree_rc_fetch_add(&B_Tree2->root->rc, 1);
    return B_Tree2;
}

static size_t B_Tree_search(const struct B_Tree *B_Tree, struct B_Tree_node *node,
    const void *key, bool *found, uint64_t *hint, int depth) 
{
    if (!hint && !B_Tree->searcher) {
        return B_Tree_node_bsearch(B_Tree, node, key, found);
    }
    if (B_Tree->searcher) {
        return B_Tree->searcher(node->items, node->nitems, key, found, 
            B_Tree->udata);
    }
    return B_Tree_node_bsearch_hint(B_Tree, node, key, found, hint, depth);
}

enum B_Tree_mut_result { 
    B_Tree_NOCHANGE,
    B_Tree_NOMEM,
    B_Tree_MUST_SPLIT,
    B_Tree_INSERTED,
    B_Tree_REPLACED,
    B_Tree_DELETED,
};

static void B_Tree_node_split(struct B_Tree *B_Tree, struct B_Tree_node *node,
    struct B_Tree_node **right, void **median) 
{
    *right = B_Tree_node_new(B_Tree, node->leaf);
    if (!*right) {
        return; // NOMEM
    }
    size_t mid = B_Tree->max_items / 2;
    *median = B_Tree_get_item_at(B_Tree, node, mid);
    (*right)->leaf = node->leaf;
    (*right)->nitems = node->nitems-(mid+1);
    memmove((*right)->items, node->items+B_Tree->elsize*(mid+1),
        (*right)->nitems*B_Tree->elsize);
    if (!node->leaf) {
        for (size_t i = 0; i <= (*right)->nitems; i++) {
            (*right)->children[i] = node->children[mid+1+i];
        }
    }
    node->nitems = mid;
}

static enum B_Tree_mut_result B_Tree_node_set(struct B_Tree *B_Tree,
    struct B_Tree_node *node, const void *item, uint64_t *hint, int depth) 
{
    bool found = false;
    size_t i = B_Tree_search(B_Tree, node, item, &found, hint, depth);
    if (found) {
        B_Tree_swap_item_at(B_Tree, node, i, item, B_Tree_SPARE_RETURN);
        return B_Tree_REPLACED;
    }
    if (node->leaf) {
        if (node->nitems == B_Tree->max_items) {
            return B_Tree_MUST_SPLIT;
        }
        B_Tree_node_shift_right(B_Tree, node, i);
        B_Tree_set_item_at(B_Tree, node, i, item);
        return B_Tree_INSERTED;
    }
    B_Tree_cow_node_or(node->children[i], return B_Tree_NOMEM);
    enum B_Tree_mut_result result = B_Tree_node_set(B_Tree, node->children[i],
        item, hint, depth+1);
    if (result == B_Tree_INSERTED || result == B_Tree_REPLACED) {
        return result;
    } else if (result == B_Tree_NOMEM) {
        return B_Tree_NOMEM;
    }
    // Split the child node
    if (node->nitems == B_Tree->max_items) {
        return B_Tree_MUST_SPLIT;
    }
    void *median = NULL;
    struct B_Tree_node *right = NULL;
    B_Tree_node_split(B_Tree, node->children[i], &right, &median);
    if (!right) {
        return B_Tree_NOMEM;
    }
    B_Tree_node_shift_right(B_Tree, node, i);
    B_Tree_set_item_at(B_Tree, node, i, median);
    node->children[i+1] = right;
    return B_Tree_node_set(B_Tree, node, item, hint, depth);
}

static void *B_Tree_set0(struct B_Tree *B_Tree, const void *item, uint64_t *hint,
    bool no_item_clone)
{
    B_Tree->oom = false;
    bool item_cloned = false;
    if (B_Tree->item_clone && !no_item_clone) {
        if (!B_Tree->item_clone(item, B_Tree_SPARE_CLONE, B_Tree->udata)) {
            goto oom;
        }
        item = B_Tree_SPARE_CLONE;
        item_cloned = true;
    }
    if (!B_Tree->root) {
        B_Tree->root = B_Tree_node_new(B_Tree, true);
        if (!B_Tree->root) {
            goto oom;
        }
        B_Tree_set_item_at(B_Tree, B_Tree->root, 0, item);
        B_Tree->root->nitems = 1;
        B_Tree->count++;
        B_Tree->height++;
        return NULL;
    }
    B_Tree_cow_node_or(B_Tree->root, goto oom);
    enum B_Tree_mut_result result;
set:
    result = B_Tree_node_set(B_Tree, B_Tree->root, item, hint, 0);
    if (result == B_Tree_REPLACED) {
        if (B_Tree->item_free) {
            B_Tree->item_free(B_Tree_SPARE_RETURN, B_Tree->udata);
        }
        return B_Tree_SPARE_RETURN;
    } else if (result == B_Tree_INSERTED) {
        B_Tree->count++;
        return NULL;
    } else if (result == B_Tree_NOMEM) {
        goto oom;
    }
    void *old_root = B_Tree->root;
    struct B_Tree_node *new_root = B_Tree_node_new(B_Tree, false);
    if (!new_root) {
        goto oom;
    }
    struct B_Tree_node *right = NULL;
    void *median = NULL;
    B_Tree_node_split(B_Tree, old_root, &right, &median);
    if (!right) {
        B_Tree->free(new_root);
        goto oom;
    }
    B_Tree->root = new_root;
    B_Tree->root->children[0] = old_root;
    B_Tree_set_item_at(B_Tree, B_Tree->root, 0, median);
    B_Tree->root->children[1] = right;
    B_Tree->root->nitems = 1;
    B_Tree->height++;
    goto set;
oom:
    if (B_Tree->item_free) {
        if (item_cloned) {
            B_Tree->item_free(B_Tree_SPARE_CLONE, B_Tree->udata);
        }
    }
    B_Tree->oom = true;
    return NULL;
}

static const void *B_Tree_get0(const struct B_Tree *B_Tree, const void *key, 
    uint64_t *hint)
{
    struct B_Tree_node *node = B_Tree->root;
    if (!node) {
        return NULL;
    }
    bool found;
    int depth = 0;
    while (1) {
        size_t i = B_Tree_search(B_Tree, node, key, &found, hint, depth);
        if (found) {
            return B_Tree_get_item_at((void*)B_Tree, node, i);
        }
        if (node->leaf) {
            return NULL;
        }
        node = node->children[i];
        depth++;
    }
}

static void B_Tree_node_rebalance(struct B_Tree *B_Tree, struct B_Tree_node *node,
    size_t i)
{
    if (i == node->nitems) {
        i--;
    }

    struct B_Tree_node *left = node->children[i];
    struct B_Tree_node *right = node->children[i+1];

    // assert(B_Tree_rc_load(&left->rc)==0);
    // assert(B_Tree_rc_load(&right->rc)==0);

    if (left->nitems + right->nitems < B_Tree->max_items) {
        // Merges the left and right children nodes together as a single node
        // that includes (left,item,right), and places the contents into the
        // existing left node. Delete the right node altogether and move the
        // following items and child nodes to the left by one slot.

        // merge (left,item,right)
        B_Tree_copy_item(B_Tree, left, left->nitems, node, i);
        left->nitems++;
        B_Tree_node_join(B_Tree, left, right);
        B_Tree->free(right);
        B_Tree_node_shift_left(B_Tree, node, i, true);
    } else if (left->nitems > right->nitems) {
        // move left -> right over one slot

        // Move the item of the parent node at index into the right-node first
        // slot, and move the left-node last item into the previously moved
        // parent item slot.
        B_Tree_node_shift_right(B_Tree, right, 0);
        B_Tree_copy_item(B_Tree, right, 0, node, i);
        if (!left->leaf) {
            right->children[0] = left->children[left->nitems];
        }
        B_Tree_copy_item(B_Tree, node, i, left, left->nitems-1);
        if (!left->leaf) {
            left->children[left->nitems] = NULL;
        }
        left->nitems--;
    } else {
        // move right -> left

        // Same as above but the other direction
        B_Tree_copy_item(B_Tree, left, left->nitems, node, i);
        if (!left->leaf) {
            left->children[left->nitems+1] = right->children[0];
        }
        left->nitems++;
        B_Tree_copy_item(B_Tree, node, i, right, 0);
        B_Tree_node_shift_left(B_Tree, right, 0, false);
    }
}

static enum B_Tree_mut_result B_Tree_node_delete(struct B_Tree *B_Tree,
    struct B_Tree_node *node, enum B_Tree_delact act, size_t index,
    const void *key, void *prev, uint64_t *hint, int depth)
{
    size_t i = 0;
    bool found = false;
    if (act == B_Tree_DELKEY) {
        i = B_Tree_search(B_Tree, node, key, &found, hint, depth);
    } else if (act == B_Tree_POPMAX) {
        i = node->nitems-1;
        found = true;
    } else if (act == B_Tree_POPFRONT) {
        i = 0;
        found = node->leaf;
    } else if (act == B_Tree_POPBACK) {
        if (!node->leaf) {
            i = node->nitems;
            found = false;
        } else {
            i = node->nitems-1;
            found = true;
        }
    }
    if (node->leaf) {
        if (found) {
            // Item was found in leaf, copy its contents and delete it.
            // This might cause the number of items to drop below min_items,
            // and it so, the caller will take care of the rebalancing.
            B_Tree_copy_item_into(B_Tree, node, i, prev);
            B_Tree_node_shift_left(B_Tree, node, i, false);
            return B_Tree_DELETED;
        }
        return B_Tree_NOCHANGE;
    }
    enum B_Tree_mut_result result;
    if (found) {
        if (act == B_Tree_POPMAX) {
            // Popping off the max item into into its parent branch to maintain
            // a balanced tree.
            i++;
            B_Tree_cow_node_or(node->children[i], return B_Tree_NOMEM);
            B_Tree_cow_node_or(node->children[i==node->nitems?i-1:i+1], 
                return B_Tree_NOMEM);
            result = B_Tree_node_delete(B_Tree, node->children[i], B_Tree_POPMAX,
                0, NULL, prev, hint, depth+1);
            if (result == B_Tree_NOMEM) {
                return B_Tree_NOMEM;
            }
            result = B_Tree_DELETED;
        } else {
            // item was found in branch, copy its contents, delete it, and 
            // begin popping off the max items in child nodes.
            B_Tree_copy_item_into(B_Tree, node, i, prev);
            B_Tree_cow_node_or(node->children[i], return B_Tree_NOMEM);
            B_Tree_cow_node_or(node->children[i==node->nitems?i-1:i+1], 
                return B_Tree_NOMEM);
            result = B_Tree_node_delete(B_Tree, node->children[i], B_Tree_POPMAX,
                0, NULL, B_Tree_SPARE_POPMAX, hint, depth+1);
            if (result == B_Tree_NOMEM) {
                return B_Tree_NOMEM;
            }
            B_Tree_set_item_at(B_Tree, node, i, B_Tree_SPARE_POPMAX);
            result = B_Tree_DELETED;
        }
    } else {
        // item was not found in this branch, keep searching.
        B_Tree_cow_node_or(node->children[i], return B_Tree_NOMEM);
        B_Tree_cow_node_or(node->children[i==node->nitems?i-1:i+1], 
            return B_Tree_NOMEM);
        result = B_Tree_node_delete(B_Tree, node->children[i], act, index, key, 
            prev, hint, depth+1);
    }
    if (result != B_Tree_DELETED) {
        return result;
    }
    if (node->children[i]->nitems < B_Tree->min_items) {
        B_Tree_node_rebalance(B_Tree, node, i);
    }
    return B_Tree_DELETED;
}

static void *B_Tree_delete0(struct B_Tree *B_Tree, enum B_Tree_delact act,
    size_t index, const void *key, uint64_t *hint) 
{
    B_Tree->oom = false;
    if (!B_Tree->root) {
        return NULL;
    }
    B_Tree_cow_node_or(B_Tree->root, goto oom);
    enum B_Tree_mut_result result = B_Tree_node_delete(B_Tree, B_Tree->root, act,
        index, key, B_Tree_SPARE_RETURN, hint, 0);
    if (result == B_Tree_NOCHANGE) {
        return NULL;
    } else if (result == B_Tree_NOMEM) {
        goto oom;
    }
    if (B_Tree->root->nitems == 0) {
        struct B_Tree_node *old_root = B_Tree->root;
        if (!B_Tree->root->leaf) {
            B_Tree->root = B_Tree->root->children[0];
        } else {
            B_Tree->root = NULL;
        }
        B_Tree->free(old_root);
        B_Tree->height--;
    }
    B_Tree->count--;
    if (B_Tree->item_free) {
        B_Tree->item_free(B_Tree_SPARE_RETURN, B_Tree->udata);
    }
    return B_Tree_SPARE_RETURN;
oom:
    B_Tree->oom = true;
    return NULL;
}

B_Tree_EXTERN
const void *B_Tree_set_hint(struct B_Tree *B_Tree, const void *item, 
    uint64_t *hint)
{
    return B_Tree_set0(B_Tree, item, hint, false);
}

B_Tree_EXTERN
const void *B_Tree_set(struct B_Tree *B_Tree, const void *item) {
    return B_Tree_set0(B_Tree, item, NULL, false);
}

B_Tree_EXTERN
const void *B_Tree_get_hint(const struct B_Tree *B_Tree, const void *key, 
    uint64_t *hint)
{
    return B_Tree_get0(B_Tree, key, hint);
}

B_Tree_EXTERN
const void *B_Tree_get(const struct B_Tree *B_Tree, const void *key) {
    return B_Tree_get0(B_Tree, key, NULL);
}

B_Tree_EXTERN
const void *B_Tree_delete_hint(struct B_Tree *B_Tree, const void *key, 
    uint64_t *hint)
{
    return B_Tree_delete0(B_Tree, B_Tree_DELKEY, 0, key, hint);
}

B_Tree_EXTERN
const void *B_Tree_delete(struct B_Tree *B_Tree, const void *key) {
    return B_Tree_delete0(B_Tree, B_Tree_DELKEY, 0, key, NULL);
}

B_Tree_EXTERN
const void *B_Tree_pop_min(struct B_Tree *B_Tree) {
    B_Tree->oom = false;
    if (B_Tree->root) {
        B_Tree_cow_node_or(B_Tree->root, goto oom);
        struct B_Tree_node *node = B_Tree->root;
        while (1) {
            if (node->leaf) {
                if (node->nitems > B_Tree->min_items) {
                    size_t i = 0;
                    B_Tree_copy_item_into(B_Tree, node, i, B_Tree_SPARE_RETURN);
                    B_Tree_node_shift_left(B_Tree, node, i, false);
                    if (B_Tree->item_free) {
                        B_Tree->item_free(B_Tree_SPARE_RETURN, B_Tree->udata);
                    }
                    B_Tree->count--;
                    return B_Tree_SPARE_RETURN;
                }
                break;
            }
            B_Tree_cow_node_or(node->children[0], goto oom);
            node = node->children[0];
        }
    }
    return B_Tree_delete0(B_Tree, B_Tree_POPFRONT, 0, NULL, NULL);
oom:
    B_Tree->oom = true;
    return NULL;
}

B_Tree_EXTERN
const void *B_Tree_pop_max(struct B_Tree *B_Tree) {
    B_Tree->oom = false;
    if (B_Tree->root) {
        B_Tree_cow_node_or(B_Tree->root, goto oom);
        struct B_Tree_node *node = B_Tree->root;
        while (1) {
            if (node->leaf) {
                if (node->nitems > B_Tree->min_items) {
                    size_t i = node->nitems-1;
                    B_Tree_copy_item_into(B_Tree, node, i, B_Tree_SPARE_RETURN);
                    node->nitems--;
                    if (B_Tree->item_free) {
                        B_Tree->item_free(B_Tree_SPARE_RETURN, B_Tree->udata);
                    }
                    B_Tree->count--;
                    return B_Tree_SPARE_RETURN;
                }
                break;
            }
            B_Tree_cow_node_or(node->children[node->nitems], goto oom);
            node = node->children[node->nitems];
        }
    }
    return B_Tree_delete0(B_Tree, B_Tree_POPBACK, 0, NULL, NULL);
oom:
    B_Tree->oom = true;
    return NULL;
}

B_Tree_EXTERN
bool B_Tree_oom(const struct B_Tree *B_Tree) {
    return !B_Tree || B_Tree->oom;
}

B_Tree_EXTERN
size_t B_Tree_count(const struct B_Tree *B_Tree) {
    return B_Tree->count;
}

B_Tree_EXTERN
int B_Tree_compare(const struct B_Tree *B_Tree, const void *a, const void *b) {
    return _B_Tree_compare(B_Tree, a, b);
}

static bool B_Tree_node_scan(const struct B_Tree *B_Tree, struct B_Tree_node *node, 
    bool (*iter)(const void *item, void *udata), void *udata)
{
    if (node->leaf) {
        for (size_t i = 0; i < node->nitems; i++) {
            if (!iter(B_Tree_get_item_at((void*)B_Tree, node, i), udata)) {
                return false;
            }
        }
        return true;
    }
    for (size_t i = 0; i < node->nitems; i++) {
        if (!B_Tree_node_scan(B_Tree, node->children[i], iter, udata)) {
            return false;
        }
        if (!iter(B_Tree_get_item_at((void*)B_Tree, node, i), udata)) {
            return false;
        }
    }
    return B_Tree_node_scan(B_Tree, node->children[node->nitems], iter, udata);
}

static bool B_Tree_node_ascend(const struct B_Tree *B_Tree,
    struct B_Tree_node *node, 
    const void *pivot, bool (*iter)(const void *item, void *udata), 
    void *udata, uint64_t *hint, int depth) 
{
    bool found;
    size_t i = B_Tree_search(B_Tree, node, pivot, &found, hint, depth);
    if (!found) {
        if (!node->leaf) {
            if (!B_Tree_node_ascend(B_Tree, node->children[i], pivot, iter, udata,
                hint, depth+1)) 
            {
                return false;
            }
        }
    }
    for (; i < node->nitems; i++) {
        if (!iter(B_Tree_get_item_at((void*)B_Tree, node, i), udata)) {
            return false;
        }
        if (!node->leaf) {
            if (!B_Tree_node_scan(B_Tree, node->children[i+1], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

B_Tree_EXTERN
bool B_Tree_ascend_hint(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), void *udata, uint64_t *hint) 
{
    if (B_Tree->root) {
        if (!pivot) {
            return B_Tree_node_scan(B_Tree, B_Tree->root, iter, udata);
        }
        return B_Tree_node_ascend(B_Tree, B_Tree->root, pivot, iter, udata, hint, 
            0);
    }
    return true;
}

B_Tree_EXTERN
bool B_Tree_ascend(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), void *udata) 
{
    return B_Tree_ascend_hint(B_Tree, pivot, iter, udata, NULL);
}

static bool B_Tree_node_reverse(const struct B_Tree *B_Tree,
    struct B_Tree_node *node, 
    bool (*iter)(const void *item, void *udata), void *udata) 
{
    if (node->leaf) {
        size_t i = node->nitems - 1;
        while (1) {
            if (!iter(B_Tree_get_item_at((void*)B_Tree, node, i), udata)) {
                return false;
            }
            if (i == 0) break;
            i--;
        }
        return true;
    }
    if (!B_Tree_node_reverse(B_Tree, node->children[node->nitems], iter, 
        udata))
    {
        return false;
    }
    size_t i = node->nitems - 1;
    while (1) {
        if (!iter(B_Tree_get_item_at((void*)B_Tree, node, i), udata)) {
            return false;
        }
        if (!B_Tree_node_reverse(B_Tree, node->children[i], iter, udata)) {
            return false;
        }
        if (i == 0) break;
        i--;
    }
    return true;
}

static bool B_Tree_node_descend(const struct B_Tree *B_Tree, 
    struct B_Tree_node *node, const void *pivot, 
    bool (*iter)(const void *item, void *udata), 
    void *udata, uint64_t *hint, int depth) 
{
    bool found;
    size_t i = B_Tree_search(B_Tree, node, pivot, &found, hint, depth);
    if (!found) {
        if (!node->leaf) {
            if (!B_Tree_node_descend(B_Tree, node->children[i], pivot, iter,
                udata, hint, depth+1)) 
            {
                return false;
            }
        }
        if (i == 0) {
            return true;
        }
        i--;
    }
    while(1) {
        if (!iter(B_Tree_get_item_at((void*)B_Tree, node, i), udata)) {
            return false;
        }
        if (!node->leaf) {
            if (!B_Tree_node_reverse(B_Tree, node->children[i], iter, udata)) {
                return false;
            }
        }
        if (i == 0) break;
        i--;
    }
    return true;
}

B_Tree_EXTERN
bool B_Tree_descend_hint(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), void *udata, uint64_t *hint) 
{
    if (B_Tree->root) {
        if (!pivot) {
            return B_Tree_node_reverse(B_Tree, B_Tree->root, iter, udata);
        }
        return B_Tree_node_descend(B_Tree, B_Tree->root, pivot, iter, udata, hint, 
            0);
    }
    return true;
}

B_Tree_EXTERN
bool B_Tree_descend(const struct B_Tree *B_Tree, const void *pivot, 
    bool (*iter)(const void *item, void *udata), void *udata) 
{
    return B_Tree_descend_hint(B_Tree, pivot, iter, udata, NULL);
}

B_Tree_EXTERN
const void *B_Tree_min(const struct B_Tree *B_Tree) {
    struct B_Tree_node *node = B_Tree->root;
    if (!node) {
        return NULL;
    }
    while (1) {
        if (node->leaf) {
            return B_Tree_get_item_at((void*)B_Tree, node, 0);
        }
        node = node->children[0];
    }
}

B_Tree_EXTERN
const void *B_Tree_max(const struct B_Tree *B_Tree) {
    struct B_Tree_node *node = B_Tree->root;
    if (!node) {
        return NULL;
    }
    while (1) {
        if (node->leaf) {
            return B_Tree_get_item_at((void*)B_Tree, node, node->nitems-1);
        }
        node = node->children[node->nitems];
    }
}

B_Tree_EXTERN
const void *B_Tree_load(struct B_Tree *B_Tree, const void *item) {
    B_Tree->oom = false;
    if (!B_Tree->root) {
        return B_Tree_set0(B_Tree, item, NULL, false);
    }
    bool item_cloned = false;
    if (B_Tree->item_clone) {
        if (!B_Tree->item_clone(item, B_Tree_SPARE_CLONE, B_Tree->udata)) {
            goto oom;
        }
        item = B_Tree_SPARE_CLONE;
        item_cloned = true;
    }
    B_Tree_cow_node_or(B_Tree->root, goto oom);
    struct B_Tree_node *node = B_Tree->root;
    while (1) {
        if (node->leaf) {
            if (node->nitems == B_Tree->max_items) break;
            void *litem = B_Tree_get_item_at(B_Tree, node, node->nitems-1);
            if (_B_Tree_compare(B_Tree, item, litem) <= 0) break;
            B_Tree_set_item_at(B_Tree, node, node->nitems, item);
            node->nitems++; 
            B_Tree->count++;
            return NULL;
        }
        B_Tree_cow_node_or(node->children[node->nitems], goto oom);
        node = node->children[node->nitems];
    }
    const void *prev = B_Tree_set0(B_Tree, item, NULL, true);
    if (!B_Tree->oom) {
        return prev;
    }
oom:
    if (B_Tree->item_free && item_cloned) {
        B_Tree->item_free(B_Tree_SPARE_CLONE, B_Tree->udata);
    }
    B_Tree->oom = true;
    return NULL;
}

B_Tree_EXTERN
size_t B_Tree_height(const struct B_Tree *B_Tree) {
    return B_Tree->height;
}

struct B_Tree_iter_stack_item {
    struct B_Tree_node *node;
    int index;
};

struct B_Tree_iter {
    struct B_Tree *B_Tree;
    void *item;
    bool seeked;
    bool atstart;
    bool atend;
    int nstack;
    struct B_Tree_iter_stack_item stack[];
};

B_Tree_EXTERN
struct B_Tree_iter *B_Tree_iter_new(const struct B_Tree *B_Tree) {
    size_t vsize = B_Tree_align_size(sizeof(struct B_Tree_iter) + 
        sizeof(struct B_Tree_iter_stack_item) * B_Tree->height);
    struct B_Tree_iter *iter = B_Tree->malloc(vsize + B_Tree->elsize);
    if (iter) {
        memset(iter, 0, vsize + B_Tree->elsize);
        iter->B_Tree = (void*)B_Tree;
        iter->item = (void*)((char*)iter + vsize);
    }
    return iter;
}

B_Tree_EXTERN
void B_Tree_iter_free(struct B_Tree_iter *iter) {
    iter->B_Tree->free(iter);
}

B_Tree_EXTERN
bool B_Tree_iter_first(struct B_Tree_iter *iter) {
    iter->atend = false;
    iter->atstart = false;
    iter->seeked = false;
    iter->nstack = 0;
    if (!iter->B_Tree->root) {
        return false;
    }
    iter->seeked = true;
    struct B_Tree_node *node = iter->B_Tree->root;
    while (1) {
        iter->stack[iter->nstack++] = (struct B_Tree_iter_stack_item) {
            .node = node,
            .index = 0,
        };
        if (node->leaf) {
            break;
        }
        node = node->children[0];
    }
    struct B_Tree_iter_stack_item *stack = &iter->stack[iter->nstack-1];
    B_Tree_copy_item_into(iter->B_Tree, stack->node, stack->index, iter->item);
    return true;
}

B_Tree_EXTERN
bool B_Tree_iter_last(struct B_Tree_iter *iter) {
    iter->atend = false;
    iter->atstart = false;
    iter->seeked = false;
    iter->nstack = 0;
    if (!iter->B_Tree->root) {
        return false;
    }
    iter->seeked = true;
    struct B_Tree_node *node = iter->B_Tree->root;
    while (1) {
        iter->stack[iter->nstack++] = (struct B_Tree_iter_stack_item) {
            .node = node,
            .index = node->nitems,
        };
        if (node->leaf) {
            iter->stack[iter->nstack-1].index--;
            break;
        }
        node = node->children[node->nitems];
    }
    struct B_Tree_iter_stack_item *stack = &iter->stack[iter->nstack-1];
    B_Tree_copy_item_into(iter->B_Tree, stack->node, stack->index, iter->item);
    return true;
}

B_Tree_EXTERN
bool B_Tree_iter_next(struct B_Tree_iter *iter) {
    if (!iter->seeked) {
        return B_Tree_iter_first(iter);
    }
    struct B_Tree_iter_stack_item *stack = &iter->stack[iter->nstack-1];
    stack->index++;
    if (stack->node->leaf) {
        if (stack->index == stack->node->nitems) {
            while (1) {
                iter->nstack--;
                if (iter->nstack == 0) {
                    iter->atend = true;
                    return false;
                }
                stack = &iter->stack[iter->nstack-1];
                if (stack->index < stack->node->nitems) {
                    break;
                }
            }
        }
    } else {
        struct B_Tree_node *node = stack->node->children[stack->index];
        while (1) {
            iter->stack[iter->nstack++] = (struct B_Tree_iter_stack_item) {
                .node = node,
                .index = 0,
            };
            if (node->leaf) {
                break;
            }
            node = node->children[0];
        }
    }
    stack = &iter->stack[iter->nstack-1];
    B_Tree_copy_item_into(iter->B_Tree, stack->node, stack->index, iter->item);
    return true;
}

B_Tree_EXTERN
bool B_Tree_iter_prev(struct B_Tree_iter *iter) {
    if (!iter->seeked) {
        return false;
    }
    struct B_Tree_iter_stack_item *stack = &iter->stack[iter->nstack-1];
    if (stack->node->leaf) {
        stack->index--;
        if (stack->index == -1) {
            while (1) {
                iter->nstack--;
                if (iter->nstack == 0) {
                    iter->atstart = true;
                    return false;
                }
                stack = &iter->stack[iter->nstack-1];
                stack->index--;
                if (stack->index > -1) {
                    break;
                }
            }
        }
    } else {
        struct B_Tree_node *node = stack->node->children[stack->index];
        while (1) {
            iter->stack[iter->nstack++] = (struct B_Tree_iter_stack_item) {
                .node = node, 
                .index = node->nitems,
            };
            if (node->leaf) {
                iter->stack[iter->nstack-1].index--;
                break;
            }
            node = node->children[node->nitems];
        }
    }
    stack = &iter->stack[iter->nstack-1];
    B_Tree_copy_item_into(iter->B_Tree, stack->node, stack->index, iter->item);
    return true;
}


B_Tree_EXTERN
bool B_Tree_iter_seek(struct B_Tree_iter *iter, const void *key) {
    iter->atend = false;
    iter->atstart = false;
    iter->seeked = false;
    iter->nstack = 0;
    if (!iter->B_Tree->root) {
        return false;
    }
    iter->seeked = true;
    struct B_Tree_node *node = iter->B_Tree->root;
    while (1) {
        bool found;
        size_t i = B_Tree_node_bsearch(iter->B_Tree, node, key, &found);
        iter->stack[iter->nstack++] = (struct B_Tree_iter_stack_item) {
            .node = node,
            .index = i,
        };
        if (found) {
            B_Tree_copy_item_into(iter->B_Tree, node, i, iter->item);
            return true;
        }
        if (node->leaf) {
            iter->stack[iter->nstack-1].index--;
            return B_Tree_iter_next(iter);
        }
        node = node->children[i];
    }
}

B_Tree_EXTERN
const void *B_Tree_iter_item(struct B_Tree_iter *iter) {
    return iter->item;
}

// 创建一颗新的存储KV_Node元素的B树
struct B_Tree *b_tree_create(){
    return B_Tree_new(sizeof(struct KV_Node), 0, kv_node_compare, NULL);
}

// 判断该B树中是否有键值为key的元素
bool b_tree_exist(const struct B_Tree *B_Tree, int key){
    struct KV_Node* my_node = NULL;
    my_node = B_Tree_get(B_Tree, &(struct KV_Node){.key = key});
    return my_node != NULL;
}

// 删除B树中键值为key值得元素(如果有)
void b_tree_erase(const struct B_Tree *B_Tree, int key){
    B_Tree_delete(B_Tree, &key);
}

// 向B树中插入(或者更新)键值为key, value值为str字符串的元素
void b_tree_insert(const struct B_Tree *B_Tree, int key, const char* s){
    B_Tree_set(B_Tree, &(struct KV_Node){.key = key, .str = strdup(s)});
}

// 返回键值key对应的B树元素, 若是无则返回NULL
KV_Node* b_tree_query(const struct B_Tree *B_Tree, int key){
    struct KV_Node* node = NULL;
    node = B_Tree_get(B_Tree, &(struct KV_Node){.key = key});
    return node;
}

// 打印B树中节点的信息
void print_node(const struct B_Tree *B_Tree, int key){
    struct KV_Node* node = NULL;
    node = B_Tree_get(B_Tree, &(struct KV_Node){.key = key});
    if(node == NULL){
        printf("<-----key值为 %d 的元素未找到----->\n", key);
        return;
    }else{
        print_kv_node(node);
    }
}

// 释放B树内存
void b_tree_free(struct B_Tree *B_Tree){
    B_Tree_free(B_Tree);
}

#ifdef B_Tree_TEST_PRIVATE_FUNCTIONS
#include "tests/priv_funcs.h"
#endif