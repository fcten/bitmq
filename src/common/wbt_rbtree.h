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
    
/* 定义红黑树结点颜色颜色类型 */
typedef enum { 
    RED = 0, 
    BLACK = 1 
} wbt_rbtree_color;

typedef struct wbt_rbtree_node_s {
    struct wbt_rbtree_node_s * left;
    struct wbt_rbtree_node_s * right;
    struct wbt_rbtree_node_s * parent;
    wbt_rbtree_color color;
    wbt_mem_t key;
    int value;
} wbt_rbtree_node_t;

typedef struct wbt_rbtree_s {
    wbt_rbtree_node_t * root;
    size_t size;
    size_t max;
} wbt_rbtree_t;

wbt_status wbt_rbtree_insert(wbt_rbtree_t *rbt, wbt_str_t *key, int value);

wbt_status wbt_rbtree_delete(wbt_rbtree_t *rbt, wbt_str_t *key);

wbt_rbtree_node_t * wbt_rbtree_get(wbt_rbtree_t *rbt, wbt_str_t *key);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_RBTREE_H__ */

