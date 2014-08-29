/* 
 * File:   wbt_connection.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午3:57
 */

#include "wbt_connection.h"
#include "wbt_string.h"
#include "wbt_log.h"
#include "wbt_heap.h"

wbt_status wbt_conn_setnonblocking(int sock);
wbt_status wbt_conn_cleanup();

int epoll_fd;
int listen_fd;

time_t cur_mtime;

/* 事件队列 */
wbt_heap_t conn_event_list;

wbt_status wbt_conn_init() {
    /* 初始化 EPOLL */
    epoll_fd = epoll_create(WBT_MAX_EVENTS);
    if(epoll_fd <= 0) {
        wbt_str_t p = wbt_string("create epoll failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }
    
    /* 初始化事件队列 */
    if(wbt_heap_new(&conn_event_list, WBT_EVENT_LIST_SIZE) != WBT_OK) {
        wbt_str_t p = wbt_string("create heap failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }
    
    wbt_heap_node_t event_cleanup;
    event_cleanup.callback = wbt_conn_cleanup;
    event_cleanup.time_out = 0;
    if(wbt_heap_insert(&conn_event_list, &event_cleanup) != WBT_OK) {
        return WBT_ERROR;
    }
    
    
    /* 初始化用于监听消息的 Socket 句柄 */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd <= 0) {
        wbt_str_t p = wbt_string("create socket failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }
    /* 把监听socket设置为非阻塞方式 */
    if( wbt_conn_setnonblocking(listen_fd) != WBT_OK ) {
        wbt_str_t p = wbt_string("set nonblocking failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }
    
    /* bind & listen */
    struct sockaddr_in sin;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(WBT_CONN_PORT);

    if(bind(listen_fd, (const struct sockaddr*)&sin, sizeof(sin)) != 0) {
        wbt_str_t p = wbt_string("bind failed.");
        wbt_log_write(p, stderr);
        
        return WBT_ERROR;
    }

    if(listen(listen_fd, WBT_CONN_BACKLOG) != 0) {
        wbt_str_t p = wbt_string("listen failed.");
        wbt_log_write(p, stderr);
        
        return WBT_ERROR;
    }
    
    /* 把监听socket加入epoll中 */
    struct epoll_event ev, events[WBT_MAX_EVENTS];
    ev.events = EPOLLIN; 
    ev.data.fd = listen_fd; 
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) { 
        wbt_str_t p = wbt_string("epoll_ctl:listen_sock failed.");
        wbt_log_write(p, stderr);
        
        return WBT_ERROR;
    }
    
    char buf[512];
    int timeout = 5000;
    struct timeval cur_utime;
    
    for (;;) {
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
            if (events[i].data.fd == listen_fd) {
                wbt_log_debug("new connection.");

                int conn_sock, addrlen;
                struct sockaddr_in remote;
                while((conn_sock = accept(listen_fd,(struct sockaddr *) &remote, 
                    (size_t *)&addrlen)) > 0)
                {
                    wbt_conn_setnonblocking(conn_sock); 

                    ev.data.fd = conn_sock;
                    ev.events = EPOLLIN | EPOLLET; 
                    
                    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock,
                        &ev) == -1)
                    {
                        wbt_str_t p = wbt_string("epoll_ctl: add failed.");
                        wbt_log_write(p, stderr);

                        return WBT_ERROR;
                    }
                }
                if (conn_sock == -1) { 
                    if (errno != EAGAIN && errno != ECONNABORTED 
                        && errno != EPROTO && errno != EINTR)
                    {
                        wbt_str_t p = wbt_string("accept failed.");
                        wbt_log_write(p, stderr);

                        return WBT_ERROR;
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                wbt_log_debug("recv data.");

                int n = 0;
                int nread;
                int bReadOk = 0;

                int fd = events[i].data.fd;

                while(1) {
                    nread = recv(fd, buf + n, 255, 0);
                    if(nread < 0) {
                        if(errno == EAGAIN) {
                            // 当前缓冲区已无数据可读
                            bReadOk = 1;
                            break;
                        } else if (errno == ECONNRESET) {
                            // 对方发送了RST
                            close(fd);
                            break;
                        } else if (errno == EINTR) {
                            // 被信号中断
                            continue;
                        } else {
                            // 其他不可弥补的错误
                            close(fd);
                            break;
                        }
                    } else if( nread == 0) {
                        // 这里表示对端的socket已正常关闭.发送过FIN了。
                        close(fd);
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
                
                
                /*while ((nread = read(fd, buf + n, 255)) > 0) { 
                    n += nread; 
                }
                wbt_log_debug("%d",n);

                if (nread == -1 && errno != EAGAIN) { 
                    perror("read error"); 
                }*/
                if( bReadOk ) {
                    ev.data.fd = fd; 
                    ev.events = EPOLLOUT | EPOLLET; 
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) { 
                        perror("epoll_ctl: mod"); 
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                wbt_log_debug("send data.");
                
                int fd = events[i].data.fd;

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

                close(fd); 
            }
        }
        
        /* 删除过期事件 */
        if(conn_event_list.size > 0) {
            wbt_heap_node_t *p = wbt_heap_get(&conn_event_list);
            while(conn_event_list.size > 0 && p->time_out <= cur_mtime) {
                wbt_heap_delete(&conn_event_list);
                /* 尝试调用回调函数 */
                if(p->callback != NULL) {
                    p->callback();
                }
                p = wbt_heap_get(&conn_event_list);
            }
            if(conn_event_list.size > 0) {
                timeout = p->time_out - cur_mtime;
            } else {
                timeout = -1;
            }
        } else {
            timeout = -1;
        }
    }
    
    close(listen_fd);
    
    return WBT_OK;
}


wbt_status wbt_conn_setnonblocking(int sock) {
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

wbt_status wbt_conn_cleanup() {
    wbt_str_t p = wbt_string("wbt_conn_cleanup.");
    wbt_log_write(p, stderr);

    wbt_heap_node_t event_cleanup;
    event_cleanup.callback = wbt_conn_cleanup;
    event_cleanup.time_out = cur_mtime + 5000;

    return wbt_heap_insert(&conn_event_list, &event_cleanup);
}