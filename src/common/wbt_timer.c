/* 
 * File:   wbt_timer.c
 * Author: fcten
 *
 * Created on 2016年2月28日, 下午6:33
 */

#include "wbt_timer.h"
#include "wbt_module.h"
#include "wbt_heap.h"
#include "wbt_log.h"

wbt_module_t wbt_module_timer = {
    wbt_string("timer"),
    wbt_timer_init,
    wbt_timer_exit
};

static wbt_heap_t wbt_timer;

wbt_status wbt_timer_init() {
    /* 初始化事件超时队列 */
    if(wbt_heap_init(&wbt_timer, WBT_EVENT_LIST_SIZE) != WBT_OK) {
        wbt_log_add("create heap failed\n");

        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_timer_exit() {
    if(wbt_timer.size > 0) {
        wbt_timer_t *p = wbt_heap_get(&wbt_timer);
        while(wbt_timer.size > 0) {
            /* 移除超时事件 */
            wbt_heap_delete(&wbt_timer);
            /* 尝试调用回调函数 */
            if( p->on_timeout != NULL ) {
                p->on_timeout(p);
            }
            p = wbt_heap_get(&wbt_timer);
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_timer_add(wbt_timer_t *timer) {
    if( !timer->on_timeout ) {
        return WBT_OK;
    }

    if( !timer->heap_idx ) {
        return wbt_heap_insert(&wbt_timer, timer);
    }
    
    return WBT_ERROR;
}

wbt_status wbt_timer_del(wbt_timer_t *timer) {
    if( timer->heap_idx ) {
        if( wbt_heap_remove(&wbt_timer, timer->heap_idx) == WBT_OK ) {
            timer->heap_idx = 0;
            timer->on_timeout = NULL;
            timer->timeout = 0;
            return WBT_OK;
        } else {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_timer_mod(wbt_timer_t *timer) {
    wbt_timer_t tmp;
    tmp.on_timeout = timer->on_timeout;
    tmp.timeout    = timer->timeout;

    if( wbt_timer_del(timer) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    timer->on_timeout = tmp.on_timeout;
    timer->timeout    = tmp.timeout;
    if( wbt_timer_add(timer) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

// 检查并执行所有的超时事件
// 返回下一次事件超时剩余时间，如果不存在则返回 0
time_t wbt_timer_process() {
    if( wbt_timer.size == 0 ) {
        return 0;
    }

    wbt_timer_t *p = wbt_heap_get(&wbt_timer);
    while(p && p->timeout <= wbt_cur_mtime ) {
        /* 移除超时事件 */
        wbt_heap_delete(&wbt_timer);
        p->heap_idx = 0;
        /* 尝试调用回调函数 */
        if(p->on_timeout != NULL) {
            p->on_timeout(p);
        }
        p = wbt_heap_get(&wbt_timer);
    }

    if(wbt_timer.size > 0) {
        return p->timeout - wbt_cur_mtime;
    } else {
        return 0;
    }
}