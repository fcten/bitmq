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
#include "wbt_module.h"

wbt_status wbt_setnonblocking(int sock);

wbt_module_t wbt_module_conn = {
    wbt_string("connection"),
    wbt_conn_init
};

int listen_fd;

extern time_t cur_mtime;

wbt_status wbt_conn_init() {
    /* 初始化用于监听消息的 Socket 句柄 */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd <= 0) {
        wbt_log_add("create socket failed\n");

        return WBT_ERROR;
    }
    /* 把监听socket设置为非阻塞方式 */
    if( wbt_setnonblocking(listen_fd) != WBT_OK ) {
        wbt_log_add("set nonblocking failed\n");

        return WBT_ERROR;
    }
    
    /* bind & listen */
    struct sockaddr_in sin;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(WBT_CONN_PORT);

    if(bind(listen_fd, (const struct sockaddr*)&sin, sizeof(sin)) != 0) {
        wbt_log_add("bind failed\n");
        
        return WBT_ERROR;
    }

    if(listen(listen_fd, WBT_CONN_BACKLOG) != 0) {
        wbt_log_add("listen failed\n");
        
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
    int conn_sock, addrlen;
    struct sockaddr_in remote;
    while((conn_sock = accept(listen_fd,(struct sockaddr *) &remote, (int *)&addrlen)) >= 0) {
        /* inet_ntoa 在 linux 下使用静态缓存实现，无需释放 */
        wbt_log_add("%s\n", inet_ntoa(remote.sin_addr));

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
            wbt_log_write(p);

            return WBT_ERROR;
        }
    }

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

    sprintf(buf.ptr, "HTTP/1.1 200 OK\r\nServer: Webit/0.1\r\nConnection: keep-alive\r\nkeep-alive: timeout=15,max=50\r\nContent-Length: 209\r\n\r\n<html><head><title>Hello World</title></head><body style=\"background-color:#f1f2f3;\"><div style=\"line-height:30px;font-size:14px;font-family: Microsoft YaHei;color:gray;\"><p>Hello World</p></div></body></html>"); 
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