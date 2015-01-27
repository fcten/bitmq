/* 
 * File:   wbt_connection.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午3:57
 */

#include <sys/sendfile.h>
#include <netinet/tcp.h>

#include "wbt_event.h"
#include "wbt_connection.h"
#include "wbt_string.h"
#include "wbt_log.h"
#include "wbt_heap.h"
#include "wbt_module.h"
#include "wbt_file.h"
#include "wbt_config.h"
#include "wbt_time.h"

wbt_status wbt_setnonblocking(int sock);

wbt_module_t wbt_module_conn = {
    wbt_string("connection"),
    wbt_conn_init,
    wbt_conn_cleanup
};

int listen_fd;

wbt_mem_t wbt_send_buf;
wbt_mem_t wbt_file_path;
wbt_mem_t wbt_default_file; 

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
    int port = 80;
    const char * listen_port = wbt_conf_get("listen");
    if( listen_port != NULL ) {
        port = atoi(listen_port);
    }
    
    struct sockaddr_in sin;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

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
    tmp_ev.trigger = wbt_on_connect;
    tmp_ev.events = EPOLLIN | EPOLLET;
    tmp_ev.fd = listen_fd;
    tmp_ev.time_out = 0;

    if(wbt_event_add(&tmp_ev) == NULL) {
        return WBT_ERROR;
    }
    
    /* 初始化 send_buf 与 file_path */
    wbt_malloc(&wbt_send_buf, 10240);
    wbt_malloc(&wbt_file_path, 512);
    wbt_malloc(&wbt_default_file, 256);
    
    wbt_mem_t * default_file = wbt_conf_get_v("default");
    wbt_memcpy(&wbt_default_file, default_file, default_file->len);
    if( default_file->len >= 255 ) {
        *((u_char *)wbt_default_file.ptr + 255) = '\0';
    } else {
        *((u_char *)wbt_default_file.ptr + default_file->len) = '\0';
    }

    return WBT_OK;
}

wbt_status wbt_conn_cleanup() {
    close(listen_fd);
    
    return WBT_OK;
}

wbt_status wbt_conn_close(wbt_event_t *ev) {
    wbt_log_debug("connection %d close.",ev->fd)

    close(ev->fd);
    ev->fd = -1;        /* close 之后 fd 会自动从 epoll 中删除 */
    wbt_event_del(ev);

    return wbt_on_close(ev);
}

wbt_status wbt_on_connect(wbt_event_t *ev) {
    struct sockaddr_in remote;
    int conn_sock, addrlen = sizeof(remote);
#ifdef WBT_USE_ACCEPT4
    while((conn_sock = accept4(listen_fd,(struct sockaddr *) &remote, (int *)&addrlen, SOCK_NONBLOCK)) >= 0) {
#else
    while((conn_sock = accept(listen_fd,(struct sockaddr *) &remote, (int *)&addrlen)) >= 0) {
        wbt_setnonblocking(conn_sock); 
#endif
        /* inet_ntoa 在 linux 下使用静态缓存实现，无需释放 */
        wbt_log_add("%s\n", inet_ntoa(remote.sin_addr));
        
        /* 发送大文件时，使用 TCP_CORK 关闭 Nagle 算法保证网络利用率 */
        /*
        int on = 1;
        setsockopt( conn_sock, SOL_TCP, TCP_CORK, &on, sizeof ( on ) );
         */

        wbt_event_t *p_ev, tmp_ev;
        tmp_ev.callback = wbt_conn_close;
        tmp_ev.trigger = wbt_on_recv;
        tmp_ev.events = EPOLLIN | EPOLLET;
        tmp_ev.fd = conn_sock;
        tmp_ev.time_out = cur_mtime + WBT_CONN_TIMEOUT;

        if((p_ev = wbt_event_add(&tmp_ev)) == NULL) {
            return WBT_ERROR;
        }
    }
    if (conn_sock == -1) { 
        if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
            wbt_log_add("accept failed\n");

            return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_on_recv(wbt_event_t *ev) {
    /* 有新的数据到达 */
    int fd = ev->fd;
    wbt_log_debug("recv data of connection %d.", fd);

    int nread;
    int bReadOk = 0;

    while( ev->data.buff.len <= 40960 ) { /* 限制数据包长度 */
        if( wbt_realloc(&ev->data.buff, ev->data.buff.len + 4096) != WBT_OK ) {
            /* 内存不足 */
            wbt_log_add("wbt_realloc failed\n");

            return WBT_ERROR;
        }
        nread = recv(fd, ev->data.buff.ptr + ev->data.buff.len - 4096, 4096, 0);
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
       if ( nread == 4096) {
           continue;   // 需要再次读取
       } else {
           // 安全读完
           bReadOk = 1;
           break; // 退出while(1),表示已经全部读完数据
       }
    }

    if( ev->data.buff.len > 40960 ) {
         /* 413 Request Entity Too Large */
         ev->data.status = STATUS_413;
    }

    if( bReadOk ) {
        /* 去除多余的缓存 */
        wbt_realloc(&ev->data.buff, ev->data.buff.len - 4096 + nread);
        wbt_http_parse(&ev->data);
        /* 需要返回响应 */
        if( ev->data.status > STATUS_UNKNOWN ) {
            /* 需要返回错误响应 */
            wbt_on_process(ev);
            /* 等待socket可写 */
            ev->trigger = wbt_on_send;
            ev->events = EPOLLOUT | EPOLLET;
            ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

            if(wbt_event_mod(ev) != WBT_OK) {
                return WBT_ERROR;
            }
        } else {
            wbt_conn_close(ev);
        }
    } else {
        /* 读取出错，或者客户端主动断开了连接 */
        wbt_conn_close(ev);
    }
    
    return WBT_OK;
}

wbt_status wbt_on_process(wbt_event_t *ev) {
    /* 生成响应消息头 */
    wbt_http_t *http = &ev->data;

    wbt_http_set_header( http, HEADER_SERVER, &header_server );
    wbt_http_set_header( http, HEADER_DATE, &wbt_time_str_http );
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE ) {
        wbt_http_set_header( http, HEADER_CONNECTION, &header_connection_keep_alive );
    } else {
        wbt_http_set_header( http, HEADER_CONNECTION, &header_connection_close );
    }
    wbt_str_t send_buf;
    if( http->status == STATUS_200 ) {
        wbt_str_t * last_modified = wbt_time_to_str( http->file.last_modified );
        if( http->bit_flag & WBT_HTTP_IF_MODIFIED_SINCE ) {
            wbt_http_header_t * header = http->headers;
            while( header != NULL ) {
                if( header->key == HEADER_IF_MODIFIED_SINCE &&
                    wbt_strcmp2( last_modified, &header->value ) == 0 ) {
                    /* 304 Not Modified */
                    http->status = STATUS_304;
                }
                header = header->next;
            }
        }

        if( http->status == STATUS_200 ) {
            send_buf = wbt_sprintf(&wbt_send_buf, "%d", http->file.size);
        } else {
            /* 目前，这里可能是 304 */
            send_buf = wbt_sprintf(&wbt_send_buf, "%d", 0);
        }
        
        wbt_http_set_header( http, HEADER_EXPIRES, &wbt_time_str_expire );
        wbt_http_set_header( http, HEADER_CACHE_CONTROL, &header_cache_control );
        
        wbt_http_set_header( http, HEADER_LAST_MODIFIED, wbt_time_to_str( http->file.last_modified ) );
    } else {
        send_buf = wbt_sprintf(&wbt_send_buf, "%d", wbt_http_error_page[http->status].len);
        
        wbt_http_set_header( http, HEADER_CONTENT_TYPE, &header_content_type_text_html );
        
        http->resp_body = wbt_http_error_page[http->status];
    }
    wbt_http_set_header( http, HEADER_CONTENT_LENGTH, &send_buf );
    
    wbt_http_generate_response_header( http );
    wbt_log_debug("%.*s", http->response.len, http->response.ptr);
    
    return WBT_OK;
}

wbt_status wbt_on_send(wbt_event_t *ev) {
    int fd = ev->fd;
    int nwrite, n;
    wbt_http_t *http = &ev->data;

    /* 如果存在 response，发送 response */
    if( http->response.len - http->resp_offset > 0 ) {
        n = http->response.len - http->resp_offset; 
        nwrite = write(fd, http->response.ptr + http->resp_offset, n); 

        if (nwrite == -1 && errno != EAGAIN) {
            wbt_conn_close(ev);
            return WBT_ERROR;
        }

        http->resp_offset += nwrite;

        wbt_log_debug("%d send, %d remain.", nwrite, http->response.len - http->resp_offset);
        if( http->response.len > http->resp_offset ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }
    
    if( http->status == STATUS_200 &&
        http->file.fd > 0 && http->file.size > http->file.offset  ) {
        // 在非阻塞模式下，对于大文件，每次只能发送一部分
        // 需要在未发送完成前继续监听可写事件
        n = http->file.size - http->file.offset;
        nwrite = sendfile( fd, http->file.fd, &http->file.offset, n );

        if (nwrite == -1 && errno != EAGAIN) { 
            wbt_conn_close(ev);
            return WBT_ERROR;
        }

        wbt_log_debug("%d send, %d remain.", nwrite, n - nwrite);
        if( n > nwrite ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }
    
    /* 所有数据发送完毕 */

    /* 如果是 keep-alive 连接，继续等待数据到来 */
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE ) {
        /* 为下一个连接初始化相关结构 */
        wbt_http_destroy( &ev->data );

        ev->events = EPOLLIN | EPOLLET;
        ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    } else {
        /* 非 keep-alive 连接，直接关闭 */
        wbt_conn_close(ev);
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