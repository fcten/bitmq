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
    // 在 gcc 中，unsigned long int 总是和指针长度一致
    unsigned long int parent_color;
    wbt_str_t key;
    wbt_str_t value;
} wbt_rbtree_node_t;

#define wbt_rbtree_parent(r)   ((wbt_rbtree_node_t *)((r)->parent_color & ~3))
#define wbt_rbtree_color(r)    ((r)->parent_color & 1)
#define wbt_rbtree_is_red(r)   (r?!wbt_rbtree_color(r):0)
#define wbt_rbtree_is_black(r) (r? wbt_rbtree_color(r):1)
#define wbt_rbtree_set_red(r)  do { (r)->parent_color &= ~1; } while (0)
#define wbt_rbtree_set_black(r)  do { (r)->parent_color |= 1; } while (0)

typedef struct wbt_rbtree_s {
    wbt_rbtree_node_t * root;
    unsigned int size;
} wbt_rbtree_t;

void wbt_rbtree_init(wbt_rbtree_t *rbt);

void wbt_rbtree_print(wbt_rbtree_node_t *node);

wbt_rbtree_node_t * wbt_rbtree_insert(wbt_rbtree_t *rbt, wbt_str_t *key);

void wbt_rbtree_delete(wbt_rbtree_t *rbt, wbt_rbtree_node_t *node);

wbt_rbtree_node_t * wbt_rbtree_get(wbt_rbtree_t *rbt, wbt_str_t *key);
void * wbt_rbtree_get_value(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_lesser(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_lesser_or_equal(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_greater(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_greater_or_equal(wbt_rbtree_t *rbt, wbt_str_t *key);

void wbt_rbtree_destroy(wbt_rbtree_t *rbt);
void wbt_rbtree_destroy_ignore_value(wbt_rbtree_t *rbt);

wbt_rbtree_node_t * wbt_rbtree_first(const wbt_rbtree_t *rbt);
wbt_rbtree_node_t * wbt_rbtree_next(const wbt_rbtree_node_t *node);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_RBTREE_H__ */

