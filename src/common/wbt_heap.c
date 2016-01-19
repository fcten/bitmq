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
wbt_status wbt_heap_init(wbt_heap_t * p, size_t max_size) {    
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
    if(p->size + 1 == p->max) {
        /* 堆已经满了，尝试扩充大小 */
        if( wbt_realloc(&p->heap, p->max * 2 * sizeof(wbt_heap_node_t))
            == WBT_OK )
        {
            p->max *= 2;
            
            wbt_log_debug("heap resize to %u", p->max);
        } else {
            wbt_log_add("heap overflow at size %u\n", p->max);

            return WBT_ERROR;
        }
    }
    
    int i;
    wbt_heap_node_t * p_node = (wbt_heap_node_t *)p->heap.ptr;
    for( i = ++ p->size; p_node[i/2].timeout > node->timeout; i /= 2 ) {
        p_node[i].timeout = p_node[i/2].timeout;
        p_node[i].ptr = p_node[i/2].ptr;
        p_node[i].modified = p_node[i/2].modified;
    }

    p_node[i].timeout = node->timeout;
    p_node[i].ptr = node->ptr;
    p_node[i].modified = node->modified;
    
    //wbt_log_debug("heap insert at %d, %d nodes.", (p_node + i), p->size);
    
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
    if( p->size == 0 ) {
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
            p_node[child+1].timeout <  p_node[child].timeout )
        {
            child++;
        }
 
        /* Percolate one level. */
        if( last_node.timeout > p_node[child].timeout ) {
            p_node[i].timeout = p_node[child].timeout;
            p_node[i].ptr = p_node[child].ptr;
            p_node[i].modified = p_node[child].modified;
        } else {
            break;
        }
    }

    p_node[i].timeout = last_node.timeout;
    p_node[i].ptr = last_node.ptr;
    p_node[i].modified = last_node.modified;
    
    //wbt_log_debug("heap delete, %d nodes.", p->size);
    // 删除元素后尝试释放空间
    // 为每一个最小堆添加定时 GC 任务太复杂了，我认为目前的做法可以接受
    if( p->max >= p->size * 4 && p->size >= 128 ) {
        wbt_realloc( &p->heap, sizeof(wbt_heap_node_t) * p->max / 2 );

        p->max /= 2;

        wbt_log_debug("heap resize to %u", p->max);
    }
    
    return WBT_OK;
}

/* 删除堆 */
wbt_status wbt_heap_destroy(wbt_heap_t * p) {
    wbt_free(&p->heap);
    p->max = 0;
    p->size = 0;
    return WBT_OK;
}

/**
 * 从最小堆定时器中删除掉超时事件
 * @param p
 */
void wbt_heap_delete_timeout(wbt_heap_t * p) {
    wbt_heap_node_t * node = wbt_heap_get(p);
    while( p->size > 0 && node->timeout <= wbt_cur_mtime ) {
        wbt_heap_delete(p);
        node = wbt_heap_get(p);
    }
}