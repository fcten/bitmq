/* 
 * File:   wbt_event.c
 * Author: Fcten
 *
 * Created on 2014年9月1日, 上午11:54
 */

#include "wbt_event.h"
#include "../common/wbt_string.h"
#include "../common/wbt_heap.h"
#include "../common/wbt_log.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_module.h"
#include "../common/wbt_time.h"
#include "../common/wbt_config.h"
#include "../common/wbt_file.h"

wbt_module_t wbt_module_event = {
    wbt_string("event"),
    wbt_event_init,
    wbt_event_exit
};

int epoll_fd;
extern wbt_socket_t wbt_listen_fd;
extern wbt_socket_t wbt_secure_fd;

extern wbt_atomic_t wbt_wating_to_exit, wbt_connection_count;

wbt_fd_t wbt_lock_accept;

/* 事件队列 */
static wbt_event_pool_t wbt_events;

/* 初始化事件队列 */
wbt_status wbt_event_init() {
    wbt_list_init(&wbt_events.node.list);
    
    wbt_events.node.pool = wbt_malloc( WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t) );
    if( wbt_events.node.pool == NULL ) {
        return WBT_ERROR;
    }

    wbt_events.available = wbt_malloc( WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t *) );
    if( wbt_events.available == NULL ) {
         return WBT_ERROR;
    }

    wbt_events.max = WBT_EVENT_LIST_SIZE;
    wbt_events.top = WBT_EVENT_LIST_SIZE - 1;
    
    /* 把所有可用内存压入栈内 */
    int i;
    for(i=0 ; i<WBT_EVENT_LIST_SIZE ; i++) {
        wbt_events.available[i] = wbt_events.node.pool + i;
    }
    
    /* 根据端口号创建锁文件 */
    wbt_str_t lock_file;
    lock_file.len = sizeof("/tmp/.wbt_accept_lock_00000");
    lock_file.str = wbt_malloc(lock_file.len);
    if( lock_file.str == NULL ) {
        return WBT_ERROR;
    }
    snprintf(lock_file.str, lock_file.len, "/tmp/.wbt_accept_lock_%d", wbt_conf.listen_port);
    
    if( ( wbt_lock_accept = wbt_lock_create(lock_file.str) ) <= 0 ) {
        wbt_log_add("create lock file failed\n");
        wbt_free(lock_file.str);
        return WBT_ERROR;
    }
    wbt_free(lock_file.str);

    /* 初始化 EPOLL */
    epoll_fd = epoll_create(WBT_MAX_EVENTS);
    if(epoll_fd <= 0) {
        wbt_log_add("create epoll failed\n");

        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_event_exit() {
    // 可以在这里删除所有残留事件
    
    return WBT_OK;
}

wbt_status wbt_event_resize() {
    wbt_event_pool_node_t *new_node = wbt_calloc(sizeof(wbt_event_pool_node_t));
    if( new_node == NULL ) {
        return WBT_ERROR;
    }

    new_node->pool = wbt_malloc( WBT_EVENT_LIST_SIZE * sizeof(wbt_event_t) );
    if( new_node->pool == NULL ) {
        wbt_free(new_node);
        return WBT_ERROR;
    }
    
    void * tmp = wbt_realloc(wbt_events.available, (wbt_events.max + WBT_EVENT_LIST_SIZE) * sizeof(wbt_event_t *));
    if( tmp == NULL ) {
        wbt_free(new_node->pool);
        wbt_free(new_node);
        return WBT_ERROR;
    }
    wbt_events.available = tmp;
    
    /* 将新的内存块加入到链表末尾 */
    wbt_list_add_tail(&new_node->list, &wbt_events.node.list);

    /* 把新的可用内存压入栈内 */
    int i;
    for(i=0 ; i<WBT_EVENT_LIST_SIZE ; i++) {
        wbt_events.available[++wbt_events.top] = new_node->pool + i;
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
    
    //wbt_log_debug("event add, fd %d, addr %p, %u events.\n", ev->fd ,ev, wbt_events.max-wbt_events.top);
    
    /* 添加到事件池内 */
    wbt_event_t *t = *(wbt_events.available + wbt_events.top);
    wbt_events.top --;
    
    t->fd      = ev->fd;
    t->on_recv = ev->on_recv;
    t->on_send = ev->on_send;
    t->events  = ev->events;
    
    t->timer.heap_idx   = 0;
    t->timer.on_timeout = ev->timer.on_timeout;
    t->timer.timeout    = ev->timer.timeout;

    t->buff = NULL;
    t->buff_len = 0;
    t->data = NULL;
    t->ctx  = NULL;
    
    t->protocol = 0;
    t->ssl = NULL;
    
    t->is_exit = 0;

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
    
    /* 如果存在超时事件，添加到超时队列中 */
    if(wbt_timer_add(&t->timer) != WBT_OK) {
        return NULL;
    }

    return t;
}

/* 删除事件 */
wbt_status wbt_event_del(wbt_event_t *ev) {
    /* 这种情况不应该发生，如果发生则说明进行了重复的删除操作 */
    if( wbt_events.top+1 >= wbt_events.max ) {
        wbt_log_debug("try to del event from empty pool\n");
        
        return WBT_ERROR;
    }
    
    //wbt_log_debug("event del, fd %d, addr %p, %u events\n", ev->fd, ev, wbt_events.max-wbt_events.top-2);

    /* 从事件池中移除 */
    wbt_events.top ++;
    wbt_events.available[wbt_events.top] = ev;

    /* 删除超时事件 */
    wbt_timer_del(&ev->timer);
    
    /* 释放可能存在的事件数据缓存 */
    wbt_free(ev->buff);
    ev->buff = NULL;
    ev->buff_len = 0;

    wbt_free(ev->data);
    ev->data = NULL;
    
    ev->protocol = 0;
    
    ev->is_exit = 0;

    /* 删除epoll事件 */
    ev->fd = -1;        /* close 之后 fd 会自动从 epoll 中删除 */
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
    //wbt_log_debug("event mod, fd %d, addr %p\n",ev->fd, ev);

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

    /* 重新添加到超时队列中 */
    if(wbt_timer_mod(&ev->timer) != WBT_OK) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_event_cleanup();

/* 事件循环 */
wbt_status wbt_event_dispatch() {;
    int timeout = 0, i = 0;
    wbt_atomic_t is_accept_lock = 0, is_accept_add = 0;
    struct epoll_event events[WBT_MAX_EVENTS];
    wbt_event_t *ev;

    wbt_event_t listen_ev, secure_ev;
    wbt_memset(&listen_ev, 0, sizeof(wbt_event_t));
    wbt_memset(&secure_ev, 0, sizeof(wbt_event_t));

    listen_ev.on_recv = wbt_on_accept;
    listen_ev.on_send = NULL;
    listen_ev.events = WBT_EV_READ | WBT_EV_ET;
    listen_ev.fd = wbt_listen_fd;

    secure_ev.on_recv = wbt_on_accept;
    secure_ev.on_send = NULL;
    secure_ev.events = WBT_EV_READ | WBT_EV_ET;
    secure_ev.fd = wbt_secure_fd;
    
    if( wbt_conf.process == 1 ) {
        //wbt_log_debug("add listen event\n");
        if(wbt_event_add(&listen_ev) == NULL) {
            return WBT_ERROR;
        }
        if(wbt_event_add(&secure_ev) == NULL) {
            return WBT_ERROR;
        }
        is_accept_add = 1;
    }
    
    while (!wbt_wating_to_exit) {
        /* 把监听socket加入epoll中 */
        if( !is_accept_add && !wbt_wating_to_exit ) {
            if( wbt_events.max-wbt_events.top == 2 ) { // TODO 判断是否有请求正在处理
                if( wbt_lock_fd(wbt_lock_accept) == 0 ) {
                    is_accept_lock = 1;
                } else {
                    is_accept_lock = 0;
                }
            } else {
                if( wbt_trylock_fd(wbt_lock_accept) == 0 ) {
                    is_accept_lock = 1;
                } else {
                    is_accept_lock = 0;
                }
            }

            if( is_accept_lock ) {
                //wbt_log_debug("add listen event\n");
                if(wbt_event_add(&listen_ev) == NULL) {
                    return WBT_ERROR;
                }
                if(wbt_event_add(&secure_ev) == NULL) {
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
        
        if( is_accept_add && wbt_wating_to_exit ) {
            // 这里未能删除监听事件，不过程序马上就要退出了，应该没有问题
            wbt_conn_close_listen();
        }
        
        int nfds = epoll_wait(epoll_fd, events, WBT_MAX_EVENTS, timeout);

        /* 更新当前时间 */
        wbt_time_update();
        wbt_time_str_update();
        
        if (nfds == -1) {
            if (errno == EINTR) {
                // 被信号中断
                wbt_log_debug("epoll_wait: Interrupted system call\n");
                continue;
            } else {
                // 其他不可弥补的错误
                wbt_log_add("epoll_wait: %s\n", strerror(errno));
                return WBT_ERROR;
            }
        }
        //wbt_log_debug("%d event happened.\n",nfds);

        if( is_accept_add ) {
            for (i = 0; i < nfds; ++i) {
                ev = (wbt_event_t *)events[i].data.ptr;

                /* 优先处理 accept */
                if( ev->fd == wbt_listen_fd ) {
                    //wbt_log_add("new conn get by %d\n", pid);
                    
                    if( ev->on_recv(ev) != WBT_OK ) {
                        wbt_log_add("call %p failed\n", ev->on_recv);
                        return WBT_ERROR;
                    }

                    if( is_accept_lock || wbt_wating_to_exit ) {
                        //wbt_log_debug("del listen event\n");
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
            if( ev->fd == wbt_listen_fd ) {
                continue;
            }
            if ((events[i].events & WBT_EV_READ) && ev->on_recv) {
                if( ev->on_recv(ev) != WBT_OK ) {
                    wbt_log_add("call %p failed\n", ev->on_recv);
                    return WBT_ERROR;
                }
            }
            if ((events[i].events & WBT_EV_WRITE) && ev->on_send) {
                if( ev->on_send(ev) != WBT_OK ) {
                    wbt_log_add("call %p failed\n", ev->on_send);
                    return WBT_ERROR;
                }
            }
        }

        /* 检查并执行超时事件 */
        timeout = wbt_timer_process();
    }

    return WBT_OK;
}