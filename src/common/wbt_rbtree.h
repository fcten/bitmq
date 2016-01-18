/* 
 * File:   wbt_rbtree.h
 * Author: Fcten
 *
 * Created on 2014年11月12日, 下午2:58
 */

#ifndef __WBT_RBTREE_H__
#define	__WBT_RBTREE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_memory.h"
#include "wbt_string.h"
#include "wbt_file.h"
    
/* 定义红黑树结点颜色颜色类型 */
typedef enum { 
    WBT_RBT_COLOR_RED = 0, 
    WBT_RBT_COLOR_BLACK = 1 
} wbt_rbtree_color;

typedef struct wbt_rbtree_node_s {
    struct wbt_rbtree_node_s * left;
    struct wbt_rbtree_node_s * right;
    struct wbt_rbtree_node_s * parent;
    wbt_rbtree_color color;
    wbt_mem_t key;
    wbt_mem_t value;
} wbt_rbtree_node_t;

typedef struct wbt_rbtree_s {
    wbt_rbtree_node_t * root;
    unsigned int size;
    unsigned int max;
} wbt_rbtree_t;

void wbt_rbtree_init(wbt_rbtree_t *rbt);

void wbt_rbtree_print(wbt_rbtree_node_t *node);

wbt_rbtree_node_t * wbt_rbtree_insert(wbt_rbtree_t *rbt, wbt_str_t *key);

void wbt_rbtree_delete(wbt_rbtree_t *rbt, wbt_rbtree_node_t *node);

wbt_rbtree_node_t * wbt_rbtree_get(wbt_rbtree_t *rbt, wbt_str_t *key);
void * wbt_rbtree_get_value(wbt_rbtree_t *rbt, wbt_str_t *key);

void wbt_rbtree_destroy(wbt_rbtree_t *rbt);
void wbt_rbtree_destroy_ignore_value(wbt_rbtree_t *rbt);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_RBTREE_H__ */

