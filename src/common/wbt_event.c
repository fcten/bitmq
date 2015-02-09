/* 
 * File:   wbt_event.c
 * Author: Fcten
 *
 * Created on 2014年9月1日, 上午11:54
 */

#include "wbt_event.h"
#include "wbt_string.h"
#include "wbt_heap.h"
#include "wbt_log.h"
#include "wbt_connection.h"
#include "wbt_module.h"
#include "wbt_time.h"

wbt_module_t wbt_module_event = {
    wbt_string("event"),
    wbt_event_init,
    wbt_event_exit
};

int epoll_fd;
extern int listen_fd;

extern int wating_to_exit;

/* 事件队列 */
wbt_event_pool_t events;

/* 事件超时队列 */
wbt_heap_t timeout_events;

/* 初始化事件队列 */
wbt_status wbt_event_init() {
    if( wbt_malloc(&events.pool, WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t)) != WBT_OK ) {
        return WBT_ERROR;
    }
    if( wbt_malloc(&events.available, WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t *)) != WBT_OK ) {
         return WBT_ERROR;
    }
    events.max = WBT_EVENT_LIST_SIZE;
    events.top = WBT_EVENT_LIST_SIZE - 1;
    
    /* 把所有可用内存压入栈内 */
    wbt_event_t *tmp_ev = events.pool.ptr;
    wbt_event_t **tmp_ev_p = events.available.ptr;
    int i;
    for(i=0 ; i<WBT_EVENT_LIST_SIZE ; i++) {
        tmp_ev_p[i] = tmp_ev + i;
    }

    /* 初始化事件超时队列 */
    if(wbt_heap_new(&timeout_events, WBT_EVENT_LIST_SIZE) != WBT_OK) {
        wbt_log_add("create heap failed\n");

        return WBT_ERROR;
    }

    return WBT_OK;
}
    
/* 程序退出前执行所有尚未触发的超时事件 */
wbt_status wbt_event_exit() {
    if(timeout_events.size > 0) {
        wbt_heap_node_t *p = wbt_heap_get(&timeout_events);
        while(timeout_events.size > 0) {
            /* 尝试调用回调函数 */
            if(p->modified == p->ev->modified && p->ev->callback != NULL) {
                p->ev->callback(p->ev);
            }
            /* 移除超时事件 */
            wbt_heap_delete(&timeout_events);
            p = wbt_heap_get(&timeout_events);
        }
    }
    
    return WBT_OK;
}

/* 添加事件 */
wbt_event_t * wbt_event_add(wbt_event_t *ev) {
    if( wating_to_exit ) return NULL;

    if( events.top == 0 ) {
        /* 事件池已满,尝试动态扩充 */
        wbt_mem_t *tmp_mem, new_mem;
        int op_status = 0;
        tmp_mem = &events.pool;
        while(tmp_mem->next != NULL) tmp_mem = tmp_mem->next;

        /* 申请空间 */
        if( wbt_malloc(&new_mem, sizeof(wbt_mem_t)) == WBT_OK ) {
            if( wbt_malloc(new_mem.ptr, WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t)) == WBT_OK ) {
                if( wbt_realloc(&events.available, (events.max + WBT_EVENT_LIST_SIZE) * sizeof(wbt_event_t *)) == WBT_OK ) {
                    /* 将新的内存块加入到链表末尾 */
                    tmp_mem->next = new_mem.ptr;
                     /* 把新的可用内存压入栈内 */
                    wbt_event_t *tmp_ev = tmp_mem->next->ptr;
                    wbt_event_t **tmp_ev_p = events.available.ptr;
                    int i;
                    for(i=0 ; i<WBT_EVENT_LIST_SIZE ; i++) {
                        events.top ++;
                        tmp_ev_p[events.top] = tmp_ev + i;
                    }

                    events.max += WBT_EVENT_LIST_SIZE;

                    op_status = 1;
                }
            }
        }
        
        if(op_status == 0) {
            wbt_log_add("event pool overflow\n");

            return NULL;
        } else {
            wbt_log_debug("event pool resize to %d", events.max);
        }
    }
    
    wbt_log_debug("event add, fd %d, addr %d, %d enents.", ev->fd ,ev, events.max-events.top);
    
    /* 添加到事件池内 */
    wbt_event_t **tmp_ev = events.available.ptr;
    /* 使用这个变量可能会提示未初始化的错误，但是无须在意 */
    unsigned int modified = tmp_ev[events.top]->modified + 1;
    *(tmp_ev[events.top]) = *(ev);
    tmp_ev[events.top]->modified = modified;
    
    /* 初始化结构体 */
    wbt_mem_t tmp;
    tmp.ptr = &tmp_ev[events.top]->data;
    tmp.len = sizeof( wbt_http_t );
    wbt_memset( &tmp, 0 );
    
    wbt_event_t *t = tmp_ev[events.top];
    events.top --;

    /* 注册epoll事件 */
    if(t->fd >= 0) {
        struct epoll_event epoll_ev;
        epoll_ev.events   = t->events;
        epoll_ev.data.ptr = t;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, t->fd, &epoll_ev) == -1) { 
            wbt_log_add("epoll_ctl:add failed\n");

            return NULL;
        }
    }
    
    /* 如果存在超时时间，添加到超时队列中 */
    if(ev->time_out >0) {
        wbt_heap_node_t timeout_ev;
        timeout_ev.ev = t;
        timeout_ev.time_out = t->time_out;
        timeout_ev.modified = t->modified;
        if(wbt_heap_insert(&timeout_events, &timeout_ev) != WBT_OK) {
            return NULL;
        }
    }

    return t;
}

/* 删除事件 */
wbt_status wbt_event_del(wbt_event_t *ev) {
    /* 这种情况不应该发生，如果发生则说明进行了重复的删除操作 */
    if( events.top+1 >= events.max ) {
        wbt_log_debug("try to del event from empty pool");
        
        return WBT_ERROR;
    }
    
    wbt_log_debug("event del, fd %d, addr %d, %d events", ev->fd, ev, events.max-events.top-2);

    /* 从事件池中移除 */
    wbt_event_t **tmp_ev = events.available.ptr;
    events.top ++;
    tmp_ev[events.top] = ev;

    /* 使超时队列中的事件过期 */
    ev->modified ++;
    
    /* 释放缓存 */
    wbt_http_destroy(&ev->data);

    /* 删除epoll事件 */
    if(ev->fd >= 0) {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ev->fd, NULL) == -1) { 
            wbt_log_add("epoll_ctl:del failed\n");

            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

/* 修改事件 */
wbt_status wbt_event_mod(wbt_event_t *ev) {
    if( wating_to_exit ) return WBT_OK;
    //wbt_log_debug("event mod, fd %d, addr %d",ev->fd,ev);

    /* 修改epoll事件 */
    if(ev->fd >= 0) {
        struct epoll_event epoll_ev;
        epoll_ev.events   = ev->events;
        epoll_ev.data.ptr = ev;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ev->fd, &epoll_ev) == -1) { 
            wbt_log_add("epoll_ctl:mod failed\n");

            return WBT_ERROR;
        }
    }
    
    /* 使超时队列中的事件过期 */
    ev->modified ++;

    /* 如果存在超时时间，重新添加到超时队列中 */
    if(ev->time_out >0) {
        wbt_heap_node_t timeout_ev;
        timeout_ev.ev = ev;
        timeout_ev.time_out = ev->time_out;
        timeout_ev.modified = ev->modified;
        if(wbt_heap_insert(&timeout_events, &timeout_ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_event_cleanup();

/* 事件循环 */
wbt_status wbt_event_dispatch() {;
    int timeout = -1;
    struct epoll_event events[WBT_MAX_EVENTS];
    wbt_event_t *ev;

    /* 初始化 EPOLL */
    epoll_fd = epoll_create(WBT_MAX_EVENTS);
    if(epoll_fd <= 0) {
        wbt_log_add("create epoll failed\n");

        return WBT_ERROR;
    }
    
    /* 把监听socket加入epoll中 */
    wbt_event_t tmp_ev;
    tmp_ev.callback = NULL;
    tmp_ev.trigger = wbt_on_connect;
    tmp_ev.events = EPOLLIN | EPOLLET;
    tmp_ev.fd = listen_fd;
    tmp_ev.time_out = 0;

    if(wbt_event_add(&tmp_ev) == NULL) {
        return WBT_ERROR;
    }

    while (!wating_to_exit) {
        int nfds = epoll_wait(epoll_fd, events, WBT_MAX_EVENTS, timeout); 
        if (nfds == -1) {
            if (errno == EINTR) {
                // 被信号中断
                wbt_log_debug("epoll_wait: Interrupted system call");
                continue;
            } else {
                // 其他不可弥补的错误
                wbt_log_add("epoll_wait: %s\n", strerror(errno));
                return WBT_ERROR;
            }
        }
        wbt_log_debug("%d event happened.",nfds);
        
        /* 更新当前时间 */
        wbt_time_update();

        int i;
        for (i = 0; i < nfds; ++i) {
            ev = (wbt_event_t *)events[i].data.ptr;

            /* 尝试调用该事件的回调函数 */
            if( ev->trigger != NULL ) {
                if( ev->trigger(ev) != WBT_OK ) {
                    return WBT_ERROR;
                }
            } else if (events[i].events & EPOLLIN) {

            } else if (events[i].events & EPOLLOUT) {

            }
        }
        
        /* 删除超时事件 */
        if(timeout_events.size > 0) {
            wbt_heap_node_t *p = wbt_heap_get(&timeout_events);
            while(timeout_events.size > 0 && p->time_out <= cur_mtime) {
                /* 尝试调用回调函数 */
                if(p->modified == p->ev->modified && p->ev->callback != NULL) {
                    p->ev->callback(p->ev);
                }
                /* 移除超时事件 */
                wbt_heap_delete(&timeout_events);
                p = wbt_heap_get(&timeout_events);
            }
            if(timeout_events.size > 0) {
                timeout = p->time_out - cur_mtime;
            } else {
                timeout = -1;
            }
        } else {
            timeout = -1;
        }
    }

    return WBT_OK;
}