/* 
 * File:   wbt_heap.c
 * Author: Fcten
 *
 * Created on 2014年8月28日, 下午2:56
 */

#include "wbt_heap.h"
#include "wbt_memory.h"
#include "wbt_log.h"

/* 建立一个空堆 */
wbt_status wbt_heap_new(wbt_heap_t * p, size_t max_size) {    
    if(wbt_malloc(&p->heap, (max_size + 1) * sizeof(wbt_heap_node_t))
        == WBT_ERROR)
    {
        return WBT_ERROR;
    }

    p->max = max_size;
    p->size = 0;
    
    return WBT_OK;
}

/* 向堆中插入一个新元素 */
wbt_status wbt_heap_insert(wbt_heap_t * p, wbt_heap_node_t * node) {
    if(p->size == p->max) {
        /* 堆已经满了 */
        wbt_str_t p = wbt_string("heap overflow.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }
    
    int i;
    wbt_heap_node_t * p_node = (wbt_heap_node_t *)p->heap.ptr;
    for( i = ++ p->size; p_node[i/2].time_out > node->time_out; i /= 2 ) {
        p_node[i].callback = p_node[i/2].callback;
        p_node[i].time_out = p_node[i/2].time_out;
        p_node[i].fd = p_node[i/2].fd;
    }
        
    p_node[i].callback = node->callback;
    p_node[i].time_out = node->time_out;
    p_node[i].fd = node->fd;
    
    wbt_log_debug("heap insert at %d, %d nodes.", (p_node + i), p->size);
    
    return WBT_OK;
}
/* 获取堆顶元素的值 */
wbt_heap_node_t * wbt_heap_get(wbt_heap_t * p) {
    wbt_heap_node_t * p_node;

    p_node = (wbt_heap_node_t *)p->heap.ptr;

    if(p->size > 0) {
        return (p_node+1);
    } else {
        return p_node;
    }
}

/* 删除堆顶元素 */
wbt_status wbt_heap_delete(wbt_heap_t * p) {
    if(p->size == 0) {
        /* 堆是空的 */
        return WBT_ERROR;
    }

    int i, child;
    wbt_heap_node_t last_node;
    wbt_heap_node_t * p_node = (wbt_heap_node_t *)p->heap.ptr;
 
    last_node = p_node[p->size];
    
    p->size --;
 
    for( i = 1; i*2 <= p->size; i = child ) {
        /* Find smaller child. */
        child = i*2;
        if( child != p->size &&
            p_node[child+1].time_out <  p_node[child].time_out )
        {
            child++;
        }
 
        /* Percolate one level. */
        if( last_node.time_out > p_node[child].time_out ) {
            p_node[i].callback = p_node[child].callback;
            p_node[i].time_out = p_node[child].time_out;
            p_node[i].fd = p_node[child].fd;
        } else {
            break;
        }
    }

    p_node[i].callback = last_node.callback;
    p_node[i].time_out = last_node.time_out;
    p_node[i].fd = last_node.fd;
    
    wbt_log_debug("heap delete, %d nodes.", p->size);
    
    return WBT_OK;
}

/* 删除堆 */
wbt_status wbt_heap_destroy(wbt_heap_t * p) {
    wbt_free(&p->heap);
    p->max = 0;
    p->size = 0;
    return WBT_OK;
}