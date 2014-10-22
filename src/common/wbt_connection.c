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

int listen_fd;

extern time_t cur_mtime;

wbt_status wbt_conn_init() {
    /* 初始化用于监听消息的 Socket 句柄 */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd <= 0) {
        wbt_str_t p = wbt_string("create socket failed.");
        wbt_log_write(p, stderr);

        return WBT_ERROR;
    }
    /* 把监听socket设置为非阻塞方式 */
    if( wbt_setnonblocking(listen_fd) != WBT_OK ) {
        wbt_str_t p = wbt_string("set nonblocking failed");
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
        wbt_str_t p = wbt_string("bind failed");
        wbt_log_write(p, stderr);
        
        return WBT_ERROR;
    }

    if(listen(listen_fd, WBT_CONN_BACKLOG) != 0) {
        wbt_str_t p = wbt_string("listen failed");
        wbt_log_write(p, stderr);
        
        return WBT_ERROR;
    }

    /* 把监听socket加入epoll中 */
    wbt_event_t tmp_ev;
    tmp_ev.callback = NULL;
    tmp_ev.events = EPOLLIN;
    tmp_ev.fd = listen_fd;
    tmp_ev.time_out = 0;

    if(wbt_event_add(&tmp_ev) == NULL) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_conn_cleanup() {
    close(listen_fd);
}

wbt_status wbt_conn_close(wbt_event_t *ev) {
    wbt_log_debug("connection %d close.",ev->fd)

    close(ev->fd);
    ev->fd = -1;        /* close 之后 fd 会自动从 epoll 中删除 */
    wbt_event_del(ev);

    return wbt_on_close(ev);
}

wbt_status wbt_on_connect(wbt_event_t *ev) {
    return WBT_OK;
}
wbt_status wbt_on_recv(wbt_event_t *ev) {
    printf("------\n%.*s\n------\n", ev->buff.len, ev->buff.ptr);

    ev->events = EPOLLOUT | EPOLLET;
    ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}
wbt_status wbt_on_send(wbt_event_t *ev) {
    int fd = ev->fd;
    wbt_mem_t buf;

    wbt_malloc(&buf, 512);

    sprintf(buf.ptr, "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nkeep-alive: timeout=15,max=50\r\nContent-Length: 209\r\n\r\n<html><head><title>Hello World</title></head><body style=\"background-color:#f1f2f3;\"><div style=\"line-height:30px;font-size:14px;font-family: Microsoft YaHei;color:gray;\"><p>Hello World</p></div></body></html>"); 
    int nwrite, data_size = strlen(buf.ptr); 
    int n = data_size; 
    while (n > 0) { 
        nwrite = write(fd, buf.ptr + data_size - n, n); 
        if (nwrite < n) { 
            if (nwrite == -1 && errno != EAGAIN) { 
                perror("write error"); 
            } 
            break; 
        } 
        n -= nwrite; 
    }
    
    wbt_free(&buf);

    ev->events = EPOLLIN | EPOLLET;
    ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    return WBT_OK;
}
wbt_status wbt_on_close(wbt_event_t *ev) {
    return WBT_OK;
}