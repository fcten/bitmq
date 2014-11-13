/* 
 * File:   wbt_rbtree.c
 * Author: Fcten
 *
 * Created on 2014年11月12日, 下午2:58
 */

#include <stdio.h>
#include "wbt_rbtree.h"
#include "wbt_memory.h"
#include "wbt_string.h"
#include "wbt_log.h"

wbt_status wbt_rbtree_insert(wbt_rbtree_t *rbt, wbt_str_t *key, int value) {
    wbt_rbtree_node_t *tmp_node, *tail_node;
    wbt_mem_t buf;

    wbt_malloc(&buf, sizeof(wbt_rbtree_node_t));
    wbt_memset(&buf, 0);

    tmp_node = (wbt_rbtree_node_t *)buf.ptr;
    wbt_malloc(&tmp_node->key, key->len);
    wbt_sprintf(&tmp_node->key, "%.*s", key->len, key->str);
    tmp_node->value = value;
    tmp_node->left = tmp_node->right = tmp_node->parent = NULL;

    if( rbt->root == NULL ) {
        rbt->root = tmp_node;
    } else {
        tail_node = rbt->root;
        while(tail_node->right != NULL) tail_node = tail_node->right;
        tmp_node->parent = tail_node;
        tail_node->right = tmp_node;
    }
    
    rbt->size ++;
    
    wbt_log_debug("rbtree insert");
    
    return WBT_OK;
}

wbt_status wbt_rbtree_delete(wbt_rbtree_t *rbt, wbt_str_t *key) {
    wbt_rbtree_node_t *node = wbt_rbtree_get(rbt, key);
    wbt_mem_t tmp;

    node->parent->right = node->right;
    
    tmp.ptr = node;
    tmp.len = sizeof(wbt_rbtree_node_t);
    
    wbt_free(&node->key);
    wbt_free(&tmp);
    
    return WBT_OK;
}

wbt_rbtree_node_t * wbt_rbtree_get(wbt_rbtree_t *rbt, wbt_str_t *key) {
    wbt_rbtree_node_t *node;
    wbt_str_t str;
    int ret;
    
    node = rbt->root;
    
    while(node != NULL) {
        str.len = node->key.len;
        str.str = node->key.ptr;

        ret = wbt_strcmp2(key, &str);
        if(ret == 0) {
            return node;
        }
        
        node = node->right;
    }
    
    return NULL;
}