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
    sin.sin_port = htons(wbt_conf.listen_port);

    if(bind(listen_fd, (const struct sockaddr*)&sin, sizeof(sin)) != 0) {
        wbt_log_add("bind failed\n");
        
        return WBT_ERROR;
    }

    if(listen(listen_fd, WBT_CONN_BACKLOG) != 0) {
        wbt_log_add("listen failed\n");
        
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_conn_cleanup() {
    close(listen_fd);
    
    return WBT_OK;
}

wbt_status wbt_conn_close(wbt_event_t *ev) {
    wbt_log_debug("connection %d close.",ev->fd);
    
    if( wbt_module_on_close(ev) != WBT_OK ) {
        // 似乎并不能做什么
    }

    close(ev->fd);
    ev->fd = -1;        /* close 之后 fd 会自动从 epoll 中删除 */
    wbt_event_del(ev);

    return WBT_OK;
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
        //wbt_log_add("%s\n", inet_ntoa(remote.sin_addr));

        wbt_event_t *p_ev, tmp_ev;
        tmp_ev.callback = wbt_conn_close;
        tmp_ev.trigger = wbt_on_recv;
        tmp_ev.events = EPOLLIN | EPOLLET;
        tmp_ev.fd = conn_sock;
        tmp_ev.time_out = cur_mtime + WBT_CONN_TIMEOUT;

        if((p_ev = wbt_event_add(&tmp_ev)) == NULL) {
            return WBT_ERROR;
        }
        
        p_ev->data.state = STATE_CONNECTION_ESTABLISHED;
        
        if( wbt_module_on_conn(p_ev) != WBT_OK ) {
            // 暂且忽略 on_conn 接口中的错误
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
    
    if( ev->data.state == STATE_CONNECTION_ESTABLISHED ||
            ev->data.state == STATE_SEND_COMPLETED ) {
        ev->data.state = STATE_RECEIVING_HEADER;
    }

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

    if( bReadOk ) {
        /* 去除多余的缓冲区 */
        wbt_realloc(&ev->data.buff, ev->data.buff.len - 4096 + nread);
        wbt_status ret = wbt_http_parse(&ev->data);
        
        /* 自定义的处理回调函数，根据 URI 返回自定义响应结果 */
        if( wbt_module_on_recv(ev) != WBT_OK ) {
            ev->data.status = 500;
        }

        if( ev->data.status > STATUS_UNKNOWN && ev->data.status < STATUS_LENGTH ) {
            /* 需要返回响应 */
            wbt_http_process(&ev->data);
            
            if( ev->data.state == STATE_GENERATING_RESPONSE ) {
                ev->data.state = STATE_READY_TO_SEND;
            }
            
            /* 等待socket可写 */
            ev->trigger = wbt_on_send;
            ev->events = EPOLLOUT | EPOLLET;
            ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

            if(wbt_event_mod(ev) != WBT_OK) {
                return WBT_ERROR;
            }
        } else if( ret == WBT_OK ) {
            /* 需要继续等待数据 */
        } else {
            /* 严重的错误，直接断开连接 */
            wbt_conn_close(ev);
        }
    } else {
        /* 读取出错，或者客户端主动断开了连接 */
        wbt_conn_close(ev);
    }
    
    return WBT_OK;
}

wbt_status wbt_on_send(wbt_event_t *ev) {
    /* 数据发送已经就绪 */
    wbt_log_debug("send data to connection %d.", ev->fd);
    
    if( ev->data.state == STATE_READY_TO_SEND ) {
        ev->data.state = STATE_SENDING;
    }

    int fd = ev->fd;
    int nwrite, n;
    wbt_http_t *http = &ev->data;
    
    /* TODO 发送大文件时，使用 TCP_CORK 关闭 Nagle 算法保证网络利用率 */
    //int on = 1;
    //setsockopt( conn_sock, SOL_TCP, TCP_CORK, &on, sizeof ( on ) );

    /* 如果存在 response，发送 response */
    if( http->response.len - http->resp_offset > 0 ) {
        n = http->response.len - http->resp_offset; 
        nwrite = write(fd, http->response.ptr + http->resp_offset, n);

        if (nwrite == -1 && errno != EAGAIN) {
            /* 这里数据发送失败了，但应当返回 WBT_OK。这个函数的返回值
             * 仅用于判断是否发生了必须重启工作进程的严重错误。
             * 如果模块需要处理数据发送失败的错误，必须根据 state 在 on_close 回调中处理。
             */
            wbt_conn_close(ev);
            
            return WBT_OK;
        }

        http->resp_offset += nwrite;

        wbt_log_debug("%d send, %d remain.", nwrite, http->response.len - http->resp_offset);
        if( http->response.len > http->resp_offset ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }
    
    if( http->status == STATUS_200 && http->file.size > http->file.offset  ) {
        // 在非阻塞模式下，对于大量数据，每次只能发送一部分
        // 需要在未发送完成前继续监听可写事件
        n = http->file.size - http->file.offset;

        if( http->file.ptr != NULL ) {
            // 需要发送的数据已经在内存中
            nwrite = write(fd, http->file.ptr + http->file.offset, n);
            http->file.offset += nwrite;
        } else if( http->file.fd > 0  ) {
            // 需要发送的数据不在内存中，但是指定了需要发送的文件
            nwrite = sendfile( fd, http->file.fd, &http->file.offset, n );
        } else {
            // 不应该出现这种情况
            n = nwrite = 0;
        }

        if (nwrite == -1 && errno != EAGAIN) { 
            /* 连接被意外关闭，同上 */
            wbt_conn_close(ev);
            
            return WBT_OK;
        }

        wbt_log_debug("%d send, %d remain.", nwrite, n - nwrite);
        if( n > nwrite ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }
    
    /* 所有数据发送完毕，调用模块接口 */
    if( ev->data.state == STATE_SENDING ) {
        ev->data.state = STATE_SEND_COMPLETED;
    }
    
    if( wbt_module_on_send(ev) != WBT_OK ) {
        // 暂且忽略 on_send 接口中的错误
    }

    /* 如果是 keep-alive 连接，继续等待数据到来 */
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE ) {
        /* 为下一个连接初始化相关结构 */
        wbt_http_destroy( &ev->data );

        ev->trigger = wbt_on_recv;
        ev->events = EPOLLIN | EPOLLET;
        ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;
        
        ev->data.state = STATE_CONNECTION_ESTABLISHED;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    } else {
        /* 非 keep-alive 连接，直接关闭 */
        wbt_conn_close(ev);
    }

    return WBT_OK;
}

/*
wbt_status wbt_on_close(wbt_event_t *ev) {
    return WBT_OK;
}
*/

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
