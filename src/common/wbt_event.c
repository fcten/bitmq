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

int epoll_fd;
extern int listen_fd;

time_t cur_mtime;

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

    /* 初始化 EPOLL */
    epoll_fd = epoll_create(WBT_MAX_EVENTS);
    if(epoll_fd <= 0) {
        wbt_str_t p = wbt_string("create epoll failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }

    /* 初始化事件超时队列 */
    if(wbt_heap_new(&timeout_events, WBT_EVENT_LIST_SIZE) != WBT_OK) {
        wbt_str_t p = wbt_string("create heap failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }

    return WBT_OK;
}

/* 添加事件 */
wbt_event_t * wbt_event_add(wbt_event_t *ev) {
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
            wbt_str_t p = wbt_string("event pool overflow");
            wbt_log_write(p, stderr);

            return NULL;
        } else {
            wbt_log_debug("event pool resize to %d", events.max);
        }
    }
    
    wbt_log_debug("event add, fd %d, addr %d, %d enents.", ev->fd ,ev, events.max-events.top);
    
    /* 添加到事件池内 */
    wbt_event_t **tmp_ev = events.available.ptr;
    tmp_ev[events.top]->callback = ev->callback;
    tmp_ev[events.top]->fd = ev->fd;
    tmp_ev[events.top]->time_out = ev->time_out;
    tmp_ev[events.top]->events = ev->events;
    tmp_ev[events.top]->modified ++;
    
    wbt_event_t *t = tmp_ev[events.top];
    events.top --;

    /* 注册epoll事件 */
    if(t->fd >= 0) {
        struct epoll_event epoll_ev;
        epoll_ev.events   = t->events;
        epoll_ev.data.ptr = t;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, t->fd, &epoll_ev) == -1) { 
            wbt_str_t p = wbt_string("epoll_ctl:add failed.");
            wbt_log_write(p, stderr);

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
    
    /* 删除epoll事件 */
    if(ev->fd >= 0) {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ev->fd, NULL) == -1) { 
            wbt_str_t p = wbt_string("epoll_ctl:del failed.");
            wbt_log_write(p, stderr);

            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

/* 修改事件 */
wbt_status wbt_event_mod(wbt_event_t *ev) {
    wbt_log_debug("event mod, fd %d, addr %d",ev->fd,ev);

    /* 修改epoll事件 */
    if(ev->fd >= 0) {
        struct epoll_event epoll_ev;
        epoll_ev.events   = ev->events;
        epoll_ev.data.ptr = ev;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ev->fd, &epoll_ev) == -1) { 
            wbt_str_t p = wbt_string("epoll_ctl:mod failed.");
            wbt_log_write(p, stderr);

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
wbt_status wbt_event_dispatch() {

    char buf[512];
    int timeout = -1;
    struct timeval cur_utime;
    struct epoll_event events[WBT_MAX_EVENTS];
    wbt_event_t *ev;
    
#ifdef WBT_DEBUG
    int for_max;
    for (for_max = 0 ; for_max < 300000 ; for_max ++) {
#else
    for (;;) {
#endif
        int nfds = epoll_wait(epoll_fd, events, WBT_MAX_EVENTS, timeout); 
        if (nfds == -1) {
            wbt_str_t p = wbt_string("epoll_wait failed.");
            wbt_log_write(p, stderr);

            return WBT_ERROR;
        }
        wbt_log_debug("%d event happened.",nfds);
        
        gettimeofday(&cur_utime, NULL);
        cur_mtime = 1000 * cur_utime.tv_sec + cur_utime.tv_usec / 1000;

        int i;
        for (i = 0; i < nfds; ++i) {
            ev = (wbt_event_t *)events[i].data.ptr;
            if (ev->fd == listen_fd) {
                int conn_sock, addrlen;
                struct sockaddr_in remote;
                while((conn_sock = accept(listen_fd,(struct sockaddr *) &remote, 
                    (size_t *)&addrlen)) >= 0)
                {
                    wbt_log_debug("new connection %d.", conn_sock);
                    wbt_setnonblocking(conn_sock); 

                    wbt_event_t *p_ev, tmp_ev;
                    tmp_ev.callback = wbt_conn_close;
                    tmp_ev.events = EPOLLIN | EPOLLET;
                    tmp_ev.fd = conn_sock;
                    tmp_ev.time_out = cur_mtime + WBT_CONN_TIMEOUT;

                    if((p_ev = wbt_event_add(&tmp_ev)) == NULL) {
                        return WBT_ERROR;
                    }
                    
                    wbt_on_connect(p_ev);
                }
                if (conn_sock == -1) { 
                    if (errno != EAGAIN && errno != ECONNABORTED 
                        && errno != EPROTO && errno != EINTR)
                    {
                        wbt_str_t p = wbt_string("accept failed");
                        wbt_log_write(p, stderr);

                        return WBT_ERROR;
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                int n = 0;
                int nread;
                int bReadOk = 0;

                wbt_event_t *ev = (wbt_event_t *)events[i].data.ptr;
                int fd = ev->fd;
                wbt_log_debug("recv data of connection %d.", fd);

                while(1) {
                    nread = recv(fd, buf + n, 255, 0);
                    if(nread < 0) {
                        if(errno == EAGAIN) {
                            // 当前缓冲区已无数据可读
                            bReadOk = 1;
                            break;
                        } else if (errno == ECONNRESET) {
                            // 对方发送了RST
                            break;
                        } else if (errno == EINTR) {
                            // 被信号中断
                            continue;
                        } else {
                            // 其他不可弥补的错误
                            break;
                        }
                    } else if( nread == 0) {
                        // 这里表示对端的socket已正常关闭.发送过FIN了。
                        break;
                    }

                    // recvNum > 0
                   n += nread;
                   if ( nread == 255) {
                       continue;   // 需要再次读取
                   } else {
                       // 安全读完
                       bReadOk = 1;
                       break; // 退出while(1),表示已经全部读完数据
                   }
                }

                if( bReadOk ) {
                    //printf("----\n%s\n----\n",buf);
                    wbt_on_recv(ev, &buf);
                } else {
                    /* 读取出错，或者客户端主动断开了连接 */
                    wbt_conn_close(ev);
                }
            } else if (events[i].events & EPOLLOUT) {
                wbt_event_t *ev = (wbt_event_t *)events[i].data.ptr;
                int fd = ev->fd;
                wbt_log_debug("send data to connection %d.", fd);

                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nHello World"); 
                int nwrite, data_size = strlen(buf); 
                int n = data_size; 
                while (n > 0) { 
                    nwrite = write(fd, buf + data_size - n, n); 
                    if (nwrite < n) { 
                        if (nwrite == -1 && errno != EAGAIN) { 
                            perror("write error"); 
                        } 
                        break; 
                    } 
                    n -= nwrite; 
                }
            }
        }
        
        /* 删除超时事件 */
        if(timeout_events.size > 0) {
            wbt_heap_node_t *p = wbt_heap_get(&timeout_events);
            while(timeout_events.size > 0 && p->time_out <= cur_mtime) {
                /* 从堆中移除超时事件 */
                wbt_heap_delete(&timeout_events);
                /* 如果该事件已经在超时前被修改过，就什么都不做 */
                if(p->modified != p->ev->modified) {
                    wbt_log_debug("modified %d %d",p->modified,p->ev->modified);
                    continue;
                }
                /* 该事件确实超时了，尝试调用回调函数 */
                if(p->ev->callback != NULL) {
                    p->ev->callback(p->ev);
                }
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

/* 将句柄设置为非阻塞 */
wbt_status wbt_setnonblocking(int sock) {
    int opts;
    opts = fcntl(sock,F_GETFL);
    if (opts < 0) {
        return WBT_ERROR;
    }
    opts = opts|O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}