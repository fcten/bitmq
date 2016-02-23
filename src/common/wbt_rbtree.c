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

/* 1）每个结点要么是红的，要么是黑的。
 * 2）根结点是黑的。
 * 3）每个叶结点，即空结点（NIL）是黑的。
 * 4）如果一个结点是红的，那么它的俩个儿子都是黑的。
 * 5）对每个结点，从该结点到其子孙结点的所有路径上包含相同数目的黑结点。
 */

/* 这是一个通用的叶节点 */
wbt_rbtree_node_t wbt_rbtree_node_nil_s = {
    &wbt_rbtree_node_nil_s,
    &wbt_rbtree_node_nil_s,
    (unsigned long int)&wbt_rbtree_node_nil_s,
    {0, NULL},
    {0, NULL}
};
wbt_rbtree_node_t *wbt_rbtree_node_nil = &wbt_rbtree_node_nil_s;

static inline void wbt_rbtree_left_rotate(wbt_rbtree_t *rbt, wbt_rbtree_node_t *node) {
    wbt_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != wbt_rbtree_node_nil) {
        wbt_rbtree_set_parent(temp->left, node);
    }

    wbt_rbtree_set_parent(temp, wbt_rbtree_parent(node));

    if (node == rbt->root) {
        rbt->root = temp;
    } else if (node == wbt_rbtree_parent(node)->left) {
        wbt_rbtree_parent(node)->left = temp;
    } else {
        wbt_rbtree_parent(node)->right = temp;
    }

    temp->left = node;
    wbt_rbtree_set_parent(node, temp);
}

static inline void wbt_rbtree_right_rotate(wbt_rbtree_t *rbt, wbt_rbtree_node_t *node) {
    wbt_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != wbt_rbtree_node_nil) {
        wbt_rbtree_set_parent(temp->right, node);
    }

    wbt_rbtree_set_parent(temp, wbt_rbtree_parent(node));

    if (node == rbt->root) {
        rbt->root = temp;
    } else if (node == wbt_rbtree_parent(node)->right) {
        wbt_rbtree_parent(node)->right = temp;
    } else {
        wbt_rbtree_parent(node)->left = temp;
    }

    temp->right = node;
    wbt_rbtree_set_parent(node, temp);
}

/* 寻找 node 的中序后继 */
static wbt_rbtree_node_t * wbt_rbtree_successor( wbt_rbtree_t * rbt, wbt_rbtree_node_t * node ) {
    if( node->right != wbt_rbtree_node_nil ) { 
        /* 如果 node 的右子树不为空，那么为右子树中最左边的结点 */
        wbt_rbtree_node_t * q = node->right; 
        wbt_rbtree_node_t * p = node->right; 
        while( p->left != wbt_rbtree_node_nil ) { 
            q = p; 
            p = p->left; 
        } 
        return q; 
    } else {
        /* 如果 node 的右子树为空，那么 node 的后继为 node 的所有祖先中为左子树的祖先 */
        wbt_rbtree_node_t * y = wbt_rbtree_parent(node); 
        while( y != wbt_rbtree_node_nil && node == y->right ) { 
            node = y; 
            y = wbt_rbtree_parent(y); 
        }
        return y; 
    } 
}

void wbt_rbtree_insert_fixup(wbt_rbtree_t *rbt, wbt_rbtree_node_t *node) {
    if( node == rbt->root ) {
        /* 情况1：插入的是根结点。
         * 原树是空树，此情况只会违反性质2。
         * 对策：直接把此结点涂为黑色。
         */
        wbt_rbtree_set_black(rbt->root);
    } else if( wbt_rbtree_is_black(wbt_rbtree_parent(node)) ) {
        /* 情况2：插入的结点的父结点是黑色。
         * 此不会违反性质2和性质4，红黑树没有被破坏。
         * 对策：什么也不做。
         */
    } else /*if( wbt_rbtree_is_red(wbt_rbtree_parent(node)) )*/ {
        /* 父节点 */
        wbt_rbtree_node_t *p_node = wbt_rbtree_parent(node);
        /* 祖父节点 */
        wbt_rbtree_node_t *pp_node = wbt_rbtree_parent(wbt_rbtree_parent(node));

        if( wbt_rbtree_is_red(pp_node->left) &&
            wbt_rbtree_is_red(pp_node->right) ) {
            /* 情况3：当前结点的父结点是红色且祖父结点的另一个子结点（叔叔结点）是红色。
             * 对策：将当前节点的父节点和叔叔节点涂黑，祖父结点涂红，把当前结点指向祖父节点，从新的当前节点重新开始算法。
             */
            wbt_rbtree_set_black(pp_node->left);
            wbt_rbtree_set_black(pp_node->right);

            wbt_rbtree_insert_fixup( rbt, pp_node );
        } else {
            if( p_node->right == node && pp_node->right == p_node ) {
                /* RR 旋转 */
                wbt_rbtree_set_black(p_node);
                wbt_rbtree_set_red(pp_node);
                
                wbt_rbtree_left_rotate( rbt, pp_node );
            } else if( p_node->left == node && pp_node->left == p_node ) {
                /* LL 旋转 */
                wbt_rbtree_set_black(p_node);
                wbt_rbtree_set_red(pp_node);

                wbt_rbtree_right_rotate( rbt, pp_node );
            } else if( p_node->left == node && pp_node->right == p_node ) {
                /* RL 旋转 */
                wbt_rbtree_right_rotate( rbt, p_node );
                /* 旋转一次之后转化为 RR 旋转 */
                wbt_rbtree_insert_fixup( rbt, p_node );
            } else /*if( p_node->right == node && pp_node->left == p_node )*/ {
                /* LR 旋转 */
                wbt_rbtree_left_rotate( rbt, p_node );
                /* 旋转一次之后转化为 LL 旋转 */
                wbt_rbtree_insert_fixup( rbt, p_node );
            }
        }
    }
}

void wbt_rbtree_init(wbt_rbtree_t *rbt) {
    /*rbt->max = 1024;*/
    rbt->size = 0;
    rbt->root = wbt_rbtree_node_nil;
}

wbt_rbtree_node_t * wbt_rbtree_insert(wbt_rbtree_t *rbt, wbt_str_t *key) {
    wbt_rbtree_node_t *tmp_node, *tail_node;
    int ret;

    tmp_node = wbt_malloc(sizeof(wbt_rbtree_node_t));
    if( tmp_node == NULL ) {
        return NULL;
    }

    tmp_node->key.str = wbt_strdup(key->str, key->len);
    tmp_node->key.len = key->len;
    if( tmp_node->key.str == NULL ) {
        wbt_free(tmp_node);
        return NULL;
    }

    tmp_node->left = tmp_node->right = wbt_rbtree_node_nil;
    wbt_rbtree_set_parent(tmp_node, wbt_rbtree_node_nil);
    wbt_rbtree_set_red(tmp_node);

    if( rbt->root == wbt_rbtree_node_nil ) {
        rbt->root = tmp_node;
    } else {
        tail_node = rbt->root;
        while(1) {
            ret = wbt_strcmp(key, (wbt_str_t *)&tail_node->key);
            if( ret == 0 ) {
                /* 键值已经存在 */
                return NULL;
            } else if( ret > 0 ) {
                if( tail_node->right != wbt_rbtree_node_nil ) {
                    tail_node = tail_node->right;
                } else {
                    /* 找到了插入位置 */
                    tail_node->right = tmp_node;
                    wbt_rbtree_set_parent(tmp_node, tail_node);
                    break;
                }
            } else {
                if( tail_node->left != wbt_rbtree_node_nil ) {
                    tail_node = tail_node->left;
                } else {
                    /* 找到了插入位置 */
                    tail_node->left = tmp_node;
                    wbt_rbtree_set_parent(tmp_node, tail_node);
                    break;
                }
            }
        }
    }
    
    /* 插入节点后需要调整 */
    wbt_rbtree_insert_fixup( rbt, tmp_node );
    
    rbt->size ++;

    //wbt_log_debug("rbtree insert\n");
    
    return tmp_node;
}

//删除黑色结点后，导致黑色缺失，违背性质4,故对树进行调整 
static void wbt_rbtree_delete_fixup(wbt_rbtree_t * rbt, wbt_rbtree_node_t * node) { 
    //如果 node 是红色，则直接把 node 变为黑色跳出循环，这样子刚好补了一重黑色,也满足了性质4 
    while( node != rbt->root && wbt_rbtree_is_black(node) ) {
        if( node == wbt_rbtree_parent(node)->left ) {
            //如果node是其父结点的左子树 
            //设w是node的兄弟结点 
            wbt_rbtree_node_t * w = wbt_rbtree_parent(node)->right;
            if( wbt_rbtree_is_red(w) ) {
                wbt_rbtree_set_black(w);
                wbt_rbtree_set_red(wbt_rbtree_parent(node));
                wbt_rbtree_left_rotate( rbt, wbt_rbtree_parent(node) ); 
                w = wbt_rbtree_parent(node)->right; 
            } 
            if( wbt_rbtree_is_black(w->left) &&
                wbt_rbtree_is_black(w->right) ) {
                wbt_rbtree_set_red(w);
                node = wbt_rbtree_parent(node); 
            } else {
                if( wbt_rbtree_is_black(w->right) ) { 
                    wbt_rbtree_set_red(w);
                    wbt_rbtree_set_black(w->left);
                    wbt_rbtree_right_rotate( rbt, w ); 
                    w = wbt_rbtree_parent(node)->right; 
                }
                wbt_rbtree_set_color(w, wbt_rbtree_color(wbt_rbtree_parent(node)));
                wbt_rbtree_set_black(wbt_rbtree_parent(node));
                wbt_rbtree_set_black(w->right);
                wbt_rbtree_left_rotate( rbt, wbt_rbtree_parent(node) ); 

                node = rbt->root;
            }
        } else {
            //对称情况，如果node是其父结点的右子树
            wbt_rbtree_node_t * w = wbt_rbtree_parent(node)->left; 
            if( wbt_rbtree_is_red(w) ) { 
                wbt_rbtree_set_black(w);
                wbt_rbtree_set_red(wbt_rbtree_parent(node));
                wbt_rbtree_right_rotate( rbt, wbt_rbtree_parent(node) ); 
                w = wbt_rbtree_parent(node)->left; 
            } 
            if( wbt_rbtree_is_black(w->left) &&
                wbt_rbtree_is_black(w->right) ) { 
                wbt_rbtree_set_red(w);
                node = wbt_rbtree_parent(node); 
            } else {
                if( wbt_rbtree_is_black(w->left) ) { 
                    wbt_rbtree_set_red(w);
                    wbt_rbtree_set_black(w->right);
                    wbt_rbtree_left_rotate( rbt, w ); 
                    w = wbt_rbtree_parent(node)->left; 
                }
                wbt_rbtree_set_color(w, wbt_rbtree_color(wbt_rbtree_parent(node)));
                wbt_rbtree_set_black(wbt_rbtree_parent(node));
                wbt_rbtree_set_black(w->left);
                wbt_rbtree_right_rotate( rbt, wbt_rbtree_parent(node) ); 

                node = rbt->root;
            }
        } 
    }
    wbt_rbtree_set_black(node);
}

// 在红黑树 rbt 中删除结点 node
void wbt_rbtree_delete(wbt_rbtree_t * rbt, wbt_rbtree_node_t * node) {
    wbt_rbtree_node_t * y; // 将要被删除的结点 
    wbt_rbtree_node_t * x; // 将要被删除的结点的唯一儿子 
    
    if( node->left == wbt_rbtree_node_nil || node->right == wbt_rbtree_node_nil ) {
        // 如果 node 有一个子树为空的话，那么将直接删除 node,即 y 指向 node
        y = node;
    } else {
        // 如果 node 的左右子树皆不为空的话，则寻找 node 的中序后继 y，
        // 用其值代替 node 的值，然后将 y 删除 ( 注意: y肯定是没有左子树的 ) 
        y = wbt_rbtree_successor(rbt, node);  
    }

    if( y->left != wbt_rbtree_node_nil ) {
        // 如果y的左子树不为空，则 x 指向 y 的左子树 
        x = y->left; 
    } else { 
        x = y->right; 
    }

    // 将原来 y 的父母设为 x 的父母，y 即将被删除
    wbt_rbtree_set_parent(x, wbt_rbtree_parent(y));

    if( wbt_rbtree_parent(y) == wbt_rbtree_node_nil ) { 
        rbt->root = x; 
    } else { 
        if( y == wbt_rbtree_parent(y)->left ) { 
            wbt_rbtree_parent(y)->left = x; 
        } else { 
            wbt_rbtree_parent(y)->right = x; 
        } 
    }

    if( y != node ) {
        // 如果被删除的结点 y 不是原来将要删除的结点 node，
        // 即只是用 y 的值来代替 node 的值，然后变相删除 y 以达到删除 node 的效果
        wbt_free( node->value.str );
        node->value = y->value;
        wbt_free( node->key.str );
        node->key = y->key;
        
        y->key.str = NULL;
        y->key.len = 0;
        y->value.str = NULL;
        y->value.len = 0;
    }

    if( wbt_rbtree_is_black(y) ) {
        // 如果被删除的结点 y 的颜色为黑色，那么可能会导致树违背性质4,导致某条路径上少了一个黑色 
        wbt_rbtree_delete_fixup(rbt, x); 
    }

    /* 删除 y */
    wbt_free(y->key.str);
    wbt_free(y->value.str);
    wbt_free(y);

    rbt->size --;
    
    //wbt_rbtree_print(rbt->root);
    //wbt_log_debug("rbtree delete\n");
} 

wbt_rbtree_node_t * wbt_rbtree_get(wbt_rbtree_t *rbt, wbt_str_t *key) {
    wbt_rbtree_node_t *node;
    int ret;
    
    node = rbt->root;
    
    while(node != wbt_rbtree_node_nil) {
        ret = wbt_strcmp(key, &node->key);
        if(ret == 0) {
            return node;
        } else if( ret > 0 ) {
            node = node->right;
        } else {
            node = node->left;
        }
    }
    
    return NULL;
}

void * wbt_rbtree_get_value(wbt_rbtree_t *rbt, wbt_str_t *key) {
    wbt_rbtree_node_t * node = wbt_rbtree_get(rbt, key);
    if( node == NULL ) {
        return NULL;
    } else {
        return node->value.str;
    }
}

wbt_rbtree_node_t * wbt_rbtree_get_lesser(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_lesser_or_equal(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_greater(wbt_rbtree_t *rbt, wbt_str_t *key);
wbt_rbtree_node_t * wbt_rbtree_get_greater_or_equal(wbt_rbtree_t *rbt, wbt_str_t *key);


void wbt_rbtree_print(wbt_rbtree_node_t *node) {
    if(node != wbt_rbtree_node_nil) {
        wbt_rbtree_print(node->left);
        wbt_log_debug("%.*s\n", node->key.len, (char *)node->key.str);
        wbt_rbtree_print(node->right);
    }
}

void wbt_rbtree_destroy_recursive(wbt_rbtree_node_t *node) {
    if(node != wbt_rbtree_node_nil) {
        wbt_rbtree_destroy_recursive(node->left);
        wbt_rbtree_destroy_recursive(node->right);
        
        wbt_free(node->key.str);
        wbt_free(node->value.str);
        wbt_free(node);
    }
}

void wbt_rbtree_destroy(wbt_rbtree_t *rbt) {
    wbt_rbtree_destroy_recursive(rbt->root);
    
    rbt->max = 0;
    rbt->size = 0;
}

void wbt_rbtree_destroy_recursive_ignore_value(wbt_rbtree_node_t *node) {
    if(node != wbt_rbtree_node_nil) {
        wbt_rbtree_destroy_recursive(node->left);
        wbt_rbtree_destroy_recursive(node->right);
        
        wbt_free(node->key.str);
        //wbt_free(node->value.str);
        wbt_free(node);
    }
}

void wbt_rbtree_destroy_ignore_value(wbt_rbtree_t *rbt) {
    wbt_rbtree_destroy_recursive_ignore_value(rbt->root);
    
    rbt->max = 0;
    rbt->size = 0;
}