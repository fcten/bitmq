/* 
 * File:   wbt_heap.h
 * Author: Fcten
 *
 * Created on 2014年8月28日, 下午2:56
 */

#ifndef __WBT_HEAP_H__
#define	__WBT_HEAP_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <time.h>

#include "../webit.h"
#include "wbt_memory.h"
#include "wbt_event.h"

typedef struct {
    time_t time_out;        /* 超时时间，距离1970年的毫秒数 */
    wbt_event_t *ev;        /* 指向事件的指针，事件存储于事件队列中 */
    unsigned int modified;  /* 添加或修改事件时事件的版本号快照 */
} wbt_heap_node_t;

typedef struct {
    wbt_mem_t heap;
    size_t size;    /* 已有元素个数 */
    size_t max;     /* 最大元素个数 */
} wbt_heap_t;

/* 建立一个空堆 */
wbt_status wbt_heap_new(wbt_heap_t * p, size_t max_size);
/* 向堆中插入一个新元素 */
wbt_status wbt_heap_insert(wbt_heap_t * p, wbt_heap_node_t * node);
/* 获取堆顶元素 */
wbt_heap_node_t * wbt_heap_get(wbt_heap_t * p);
/* 删除堆顶元素 */
wbt_status wbt_heap_delete(wbt_heap_t * p);
/* 删除堆 */
wbt_status wbt_heap_destroy(wbt_heap_t * p);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_HEAP_H__ */

