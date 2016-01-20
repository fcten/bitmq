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
#include "wbt_config.h"

wbt_module_t wbt_module_event = {
    wbt_string("event"),
    wbt_event_init,
    wbt_event_exit
};

int epoll_fd;
extern int listen_fd;

extern wbt_atomic_t wbt_wating_to_exit, wbt_connection_count;

int wbt_lock_accept;

/* 事件队列 */
wbt_event_pool_t wbt_events;

/* 事件超时队列 */
wbt_heap_t timeout_events;

/* 初始化事件队列 */
wbt_status wbt_event_init() {
    wbt_list_init(&wbt_events.node.list);
    
    if( wbt_malloc(&wbt_events.node.pool, WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t)) != WBT_OK ) {
        return WBT_ERROR;
    }
    if( wbt_malloc(&wbt_events.available, WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t *)) != WBT_OK ) {
         return WBT_ERROR;
    }
    wbt_events.max = WBT_EVENT_LIST_SIZE;
    wbt_events.top = WBT_EVENT_LIST_SIZE - 1;
    
    /* 把所有可用内存压入栈内 */
    wbt_event_t *tmp_ev = wbt_events.node.pool.ptr;
    wbt_event_t **tmp_ev_p = wbt_events.available.ptr;
    int i;
    for(i=0 ; i<WBT_EVENT_LIST_SIZE ; i++) {
        tmp_ev_p[i] = tmp_ev + i;
    }

    /* 初始化事件超时队列 */
    if(wbt_heap_init(&timeout_events, WBT_EVENT_LIST_SIZE) != WBT_OK) {
        wbt_log_add("create heap failed\n");

        return WBT_ERROR;
    }
    
    /* 根据端口号创建锁文件 */
    wbt_mem_t lock_file;
    if( wbt_malloc(&lock_file, sizeof("/tmp/.wbt_accept_lock_00000")) != WBT_OK ) {
        return WBT_ERROR;
    }
    wbt_sprintf(&lock_file, "/tmp/.wbt_accept_lock_%d", wbt_conf.listen_port);
    
    if( ( wbt_lock_accept = wbt_lock_create(lock_file.ptr) ) <= 0 ) {
        wbt_log_add("create lock file failed\n");
        wbt_free(&lock_file);
        return WBT_ERROR;
    }
    wbt_free(&lock_file);

    return WBT_OK;
}
    
/* 程序退出前执行所有尚未触发的超时事件 */
wbt_status wbt_event_exit() {
    if(timeout_events.size > 0) {
        wbt_heap_node_t *p = wbt_heap_get(&timeout_events);
        while(timeout_events.size > 0) {
            /* 尝试调用回调函数 */
            if(p->modified == ((wbt_event_t *)(p->ptr))->modified
                    && ((wbt_event_t *)(p->ptr))->on_timeout != NULL) {
                ((wbt_event_t *)(p->ptr))->on_timeout(p->ptr);
            }
            /* 移除超时事件 */
            wbt_heap_delete(&timeout_events);
            p = wbt_heap_get(&timeout_events);
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_event_resize() {
    wbt_mem_t tmp;

    if( wbt_malloc(&tmp, sizeof(wbt_event_pool_node_t)) != WBT_OK ) {
        return WBT_ERROR;
    }
    wbt_event_pool_node_t *new_node = tmp.ptr;

    if( wbt_malloc(&new_node->pool, WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t)) != WBT_OK ) {
        wbt_free(&tmp);
        return WBT_ERROR;
    }
    
    if( wbt_realloc(&wbt_events.available, (wbt_events.max + WBT_EVENT_LIST_SIZE) * sizeof(wbt_event_t *)) != WBT_OK ) {
        wbt_free(&new_node->pool);
        wbt_free(&tmp);
        return WBT_ERROR;
    }
    
    /* 将新的内存块加入到链表末尾 */
    wbt_list_add_tail(&new_node->list, &wbt_events.node.list);

    /* 把新的可用内存压入栈内 */
    wbt_event_t *tmp_ev = new_node->pool.ptr;
    wbt_event_t **tmp_ev_p = wbt_events.available.ptr;
    int i;
    for(i=0 ; i<WBT_EVENT_LIST_SIZE ; i++) {
        tmp_ev_p[++wbt_events.top] = tmp_ev + i;
    }

    wbt_events.max += WBT_EVENT_LIST_SIZE;
    
    return WBT_OK;
}

/* 添加事件 */
wbt_event_t * wbt_event_add(wbt_event_t *ev) {
    if( wbt_events.top == 0 ) {
        /* 事件池已满,尝试动态扩充 */
        if(wbt_event_resize() != WBT_OK) {
            wbt_log_add("event pool overflow\n");

            return NULL;
        } else {
            wbt_log_add("event pool resize to %u\n", wbt_events.max);
        }
    }
    
    wbt_log_debug("event add, fd %d, addr %p, %u events.", ev->fd ,ev, wbt_events.max-wbt_events.top);
    
    /* 添加到事件池内 */
    wbt_event_t *t = *((wbt_event_t **)wbt_events.available.ptr + wbt_events.top);
    wbt_events.top --;
    
    t->fd         = ev->fd;
    t->on_timeout = ev->on_timeout;
    t->on_recv    = ev->on_recv;
    t->on_send    = ev->on_send;
    t->timeout    = ev->timeout;
    t->events     = ev->events;

    t->modified ++;

    t->buff.ptr = NULL;
    t->buff.len = 0;
    
    t->data.ptr = NULL;
    t->data.len = 0;
    
    t->ctx = NULL;

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
    if(t->timeout > 0) {
        wbt_heap_node_t timeout_ev;
        timeout_ev.ptr = t;
        timeout_ev.timeout = t->timeout;
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
    if( wbt_events.top+1 >= wbt_events.max ) {
        wbt_log_debug("try to del event from empty pool");
        
        return WBT_ERROR;
    }
    
    wbt_log_debug("event del, fd %d, addr %p, %u events", ev->fd, ev, wbt_events.max-wbt_events.top-2);

    /* 从事件池中移除 */
    wbt_event_t **tmp_ev = wbt_events.available.ptr;
    wbt_events.top ++;
    tmp_ev[wbt_events.top] = ev;

    /* 使超时队列中的事件过期 */
    ev->modified ++;
    
    /* 释放可能存在的事件数据缓存 */
    wbt_free(&ev->buff);
    wbt_free(&ev->data);

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
    //wbt_log_debug("event mod, fd %d, addr %d",ev->fd,ev);

    /* 修改epoll事件 */
    if(ev->fd >= 0) {
        struct epoll_event epoll_ev;
        epoll_ev.events   = ev->events;
        epoll_ev.data.ptr = ev;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ev->fd, &epoll_ev) == -1) { 
            wbt_log_add("epoll_ctl:mod failed %d\n", ev->fd);

            return WBT_ERROR;
        }
    }
    
    /* 使超时队列中的事件过期 */
    /* 采用这样的设计，而不是从超时队列中删除对应事件，是因为从最小堆中删除特定事件
     * 需要首先知道该事件的位置，而由于最小堆中元素的位置在频繁地变化，我们将不得不
     * 在每次删除前搜索整个堆。
     * 不过，这样做的坏处是，每一个请求都会产生数个超时事件并残留在超时队列中 15 秒
     * （默认的 event_timeout 设定），对于一个繁忙的服务器，这意味着数百万个已失效的
     * 超时事件会残留在超时队列中，使得插入操作花费额外的时间，并占用额外的服务器内
     * 存。 */
    ev->modified ++;

    /* 如果存在超时时间，重新添加到超时队列中 */
    if(ev->timeout >0) {
        wbt_heap_node_t timeout_ev;
        timeout_ev.ptr = ev;
        timeout_ev.timeout  = ev->timeout;
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
    int timeout = -1, i = 0;
    wbt_atomic_t is_accept_lock = 0, is_accept_add = 0;
    struct epoll_event events[WBT_MAX_EVENTS];
    wbt_event_t *ev;

    /* 初始化 EPOLL */
    epoll_fd = epoll_create(WBT_MAX_EVENTS);
    if(epoll_fd <= 0) {
        wbt_log_add("create epoll failed\n");

        return WBT_ERROR;
    }
    
    pid_t pid = getpid();

    wbt_event_t listen_ev;
    listen_ev.on_timeout = NULL;
    listen_ev.on_recv = wbt_on_connect;
    listen_ev.on_send = NULL;
    listen_ev.events = EPOLLIN | EPOLLET;
    listen_ev.fd = listen_fd;
    listen_ev.timeout = 0;
    
    if( wbt_conf.process == 1 ) {
        wbt_log_debug("add listen event");
        if(wbt_event_add(&listen_ev) == NULL) {
            return WBT_ERROR;
        }
        is_accept_add = 1;
    }
    
    while (!wbt_wating_to_exit || wbt_connection_count) {
        /* 把监听socket加入epoll中 */
        if( !is_accept_add && !wbt_wating_to_exit ) {
            if( wbt_events.max-wbt_events.top == 2 ) { // TODO 判断是否有请求正在处理
                if( wbt_lock_fd(wbt_lock_accept) == WBT_OK ) {
                    is_accept_lock = 1;
                } else {
                    is_accept_lock = 0;
                }
            } else {
                if( wbt_trylock_fd(wbt_lock_accept) == WBT_OK ) {
                    is_accept_lock = 1;
                } else {
                    is_accept_lock = 0;
                }
            }

            if( is_accept_lock ) {
                wbt_log_debug("add listen event");
                if(wbt_event_add(&listen_ev) == NULL) {
                    return WBT_ERROR;
                }
                is_accept_add = 1;
            } else {
                // 如果未能抢占 listen fd，等待一段时间重新尝试抢占 
                if( timeout > 100 * wbt_conf.process || timeout == -1 ) {
                    timeout = 100 * wbt_conf.process;
                }
            }
        }

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

        if( is_accept_add ) {
            for (i = 0; i < nfds; ++i) {
                ev = (wbt_event_t *)events[i].data.ptr;

                /* 优先处理 accept */
                if( ev->fd == listen_fd ) {
                    //wbt_log_add("new conn get by %d\n", pid);
                    
                    if( ev->on_recv(ev) != WBT_OK ) {
                        wbt_log_add("call %p failed\n", ev->on_recv);
                        return WBT_ERROR;
                    }

                    if( is_accept_lock || wbt_wating_to_exit ) {
                        wbt_log_debug("del listen event");
                        if( wbt_event_del(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        is_accept_add = 0;
                        wbt_unlock_fd(wbt_lock_accept);
                        is_accept_lock = 0;
                    }
                    break;
                }
            }
        }

        for (i = 0; i < nfds; ++i) {
            ev = (wbt_event_t *)events[i].data.ptr;

            /* 尝试调用该事件的回调函数 */
            if( ev->fd == listen_fd ) {
                continue;
            } else if (events[i].events & EPOLLRDHUP) {
                wbt_conn_close(ev);
            } else if ((events[i].events & EPOLLIN) && ev->on_recv) {
                if( ev->on_recv(ev) != WBT_OK ) {
                    wbt_log_add("call %p failed\n", ev->on_recv);
                    return WBT_ERROR;
                }
            } else if ((events[i].events & EPOLLOUT) && ev->on_send) {
                if( ev->on_send(ev) != WBT_OK ) {
                    wbt_log_add("call %p failed\n", ev->on_send);
                    return WBT_ERROR;
                }
            }
        }
        
        /* 删除超时事件 */
        if(timeout_events.size > 0) {
            wbt_heap_node_t *p = wbt_heap_get(&timeout_events);
            while(timeout_events.size > 0 &&
                    (p->timeout <= wbt_cur_mtime || p->modified != ((wbt_event_t *)(p->ptr))->modified)) {
                /* 尝试调用回调函数 */
                if(p->modified == ((wbt_event_t *)(p->ptr))->modified && ((wbt_event_t *)(p->ptr))->on_timeout != NULL) {
                    ((wbt_event_t *)(p->ptr))->on_timeout(p->ptr);
                }
                /* 移除超时事件 */
                wbt_heap_delete(&timeout_events);
                p = wbt_heap_get(&timeout_events);
            }
            if(timeout_events.size > 0) {
                timeout = p->timeout - wbt_cur_mtime;
            } else {
                timeout = -1;
            }
        } else {
            timeout = -1;
        }
    }

    return WBT_OK;
}