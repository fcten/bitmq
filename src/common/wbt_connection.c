/* 
 * File:   wbt_connection.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午3:57
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../webit.h"
#include "../event/wbt_event.h"
#include "wbt_connection.h"
#include "wbt_string.h"
#include "wbt_log.h"
#include "wbt_heap.h"
#include "wbt_module.h"
#include "wbt_file.h"
#include "wbt_config.h"
#include "wbt_time.h"
#include "wbt_ssl.h"

wbt_atomic_t wbt_connection_count = 0;

wbt_module_t wbt_module_conn = {
    wbt_string("connection"),
    wbt_conn_init,
    wbt_conn_cleanup
};

wbt_socket_t wbt_listen_fd = -1;

wbt_status wbt_conn_init() {
    // TODO linux 3.9 以上内核支持 REUSE_PORT，可以优化多核性能
    
    /* 初始化用于监听消息的 Socket 句柄 */
    char * listen_fd_env = getenv("WBT_LISTEN_FD");
    if(listen_fd_env == NULL) {
        wbt_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(wbt_listen_fd <= 0) {
            wbt_log_add("create socket failed\n");

            return WBT_ERROR;
        }
        /* 把监听socket设置为非阻塞方式 */
        if( wbt_nonblocking(wbt_listen_fd) == -1 ) {
            wbt_log_add("set nonblocking failed\n");

            return WBT_ERROR;
        }

        /* 在重启程序以及进行热更新时，避免 TIME_WAIT 和 CLOSE_WAIT 状态的连接导致 bind 失败 */
        int on = 1; 
        if(setsockopt(wbt_listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0) {  
            wbt_log_add("set SO_REUSEADDR failed\n");  

            return WBT_ERROR;
        }

        /* bind & listen */    
        struct sockaddr_in sin;
        wbt_memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons(wbt_conf.listen_port);

        if(bind(wbt_listen_fd, (const struct sockaddr*)&sin, sizeof(sin)) != 0) {
            wbt_log_add("bind failed\n");

            return WBT_ERROR;
        }

        if(listen(wbt_listen_fd, WBT_CONN_BACKLOG) != 0) {
            wbt_log_add("listen failed\n");

            return WBT_ERROR;
        }
    } else {
        wbt_listen_fd = atoi(listen_fd_env);
    }
    
    wbt_log_add("listen fd: %d\n", wbt_listen_fd);

    return WBT_OK;
}

wbt_status wbt_conn_cleanup() {
    return wbt_conn_close_listen();
}

wbt_status wbt_conn_close_listen() {
    if( wbt_listen_fd >= 0 ) {
		wbt_close_socket(wbt_listen_fd);
        wbt_listen_fd = -1;
    }
    
    return WBT_OK;
}

wbt_status wbt_conn_close(wbt_timer_t *timer) {
    wbt_event_t *ev = wbt_timer_entry(timer, wbt_event_t, timer);

    return wbt_on_close(ev);
}

wbt_status wbt_on_close(wbt_event_t *ev) {
    //wbt_log_debug("connection %d close.",ev->fd);
    
    if( wbt_module_on_close(ev) != WBT_OK ) {
        // 似乎并不能做什么
    }

	wbt_close_socket(ev->fd);
    wbt_event_del(ev);
    
    wbt_connection_count --;

    return WBT_OK;
}

wbt_status wbt_on_accept(wbt_event_t *ev) {
    struct sockaddr_in remote;
    int addrlen = sizeof(remote);
    wbt_socket_t conn_sock;
#ifdef WBT_USE_ACCEPT4
    while((int)(conn_sock = accept4(wbt_listen_fd,(struct sockaddr *) &remote, (int *)&addrlen, SOCK_NONBLOCK)) >= 0) {
#else
    while((int)(conn_sock = accept(wbt_listen_fd,(struct sockaddr *) &remote, (int *)&addrlen)) >= 0) {
        wbt_nonblocking(conn_sock); 
#endif
        /* inet_ntoa 在 linux 下使用静态缓存实现，无需释放 */
        //wbt_log_add("%s\n", inet_ntoa(remote.sin_addr));

        wbt_event_t *p_ev, tmp_ev;
        tmp_ev.timer.on_timeout = wbt_conn_close;
        tmp_ev.timer.timeout    = wbt_cur_mtime + wbt_conf.event_timeout;
        tmp_ev.on_recv = wbt_on_recv;
        tmp_ev.on_send = wbt_on_send;
        tmp_ev.events  = WBT_EV_READ | WBT_EV_ET;
        tmp_ev.fd      = conn_sock;

        if((p_ev = wbt_event_add(&tmp_ev)) == NULL) {
            continue;
        }
        
        wbt_connection_count ++;

        // TODO 待优化，事实上目前这里只用于调用 SSL 模块
        if( wbt_module_on_conn(p_ev) != WBT_OK ) {
            wbt_on_close(p_ev);
            continue;
        }
    }

    if (conn_sock == -1) {
        wbt_err_t err = wbt_socket_errno;

        if (err == WBT_EAGAIN) {
            return WBT_OK;
        } else if (err == WBT_ECONNABORTED) {
            wbt_log_add("accept failed\n");

            return WBT_ERROR;
        } else if (err == WBT_EMFILE || err == WBT_ENFILE) {
            wbt_log_add("accept failed\n");

            return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_on_recv(wbt_event_t *ev) {
    /* 有新的数据到达 */
    //wbt_log_debug("recv data of connection %d.", fd);
    
    int nread;

    /* 无论采用何种协议， Webit 统一限定单个请求的上限为 WBT_MAX_PROTO_BUF_LEN（64K）。
     * 如果单个请求大小超过 64K，Webit 将会立即关闭该连接。
     * 
     * Webit 统一为每个连接分配 64K 固定长度的缓冲区。如果请求尚未接收完整，Webit 会保
     * 留完整的缓冲区，并在 ev->buff_len 中保存已接收的长度。否则，Webit 会释放整个缓冲
     * 区，确保在维持大量空闲连接时尽量节省内存。
     */
    if( ev->buff == NULL ) {
        ev->buff = wbt_malloc(WBT_MAX_PROTO_BUF_LEN);
        ev->buff_len = 0;
        
        if( ev->buff == NULL ) {
            /* 内存不足 */
            wbt_on_close(ev);
            return WBT_OK;
        }
    }

    nread = wbt_recv(ev, (unsigned char *)ev->buff + ev->buff_len,
        WBT_MAX_PROTO_BUF_LEN - ev->buff_len);

    if(nread < 0) {
        wbt_err_t err = wbt_socket_errno;
        if(err == WBT_EAGAIN) {
            // 当前缓冲区已无数据可读
        } else if (err == WBT_ECONNRESET) {
            // 对方发送了RST
            wbt_log_add("connection close: connection reset by peer\n");
            wbt_on_close(ev);
            return WBT_OK;
        } else {
            // 其他不可弥补的错误
            wbt_log_add("connection close: %d\n", err);
            wbt_on_close(ev);
            return WBT_OK;
        }
    } else if(nread == 0) {
        wbt_log_add("connection close: connection closed by peer\n");
        wbt_on_close(ev);
        return WBT_OK;
    } else {
        ev->buff_len += nread;
    }
    
    if( ev->protocol == WBT_PROTOCOL_UNKNOWN ) {
        if(ev->buff_len < 5) {
            return WBT_OK;
        }
        
        char * buff = (char *)ev->buff;
        
        // 协议分析
        if( buff[1] == 'B' &&
            buff[2] == 'M' &&
            buff[3] == 'T' &&
            buff[4] == 'P' ) {
            ev->protocol = WBT_PROTOCOL_BMTP;
        } else if(1) {
            /* TODO 判断 HTTP 协议 */
            ev->protocol = WBT_PROTOCOL_HTTP;
        } else {
            wbt_log_add("connection close: unknown protocol\n");
            wbt_on_close(ev);
            return WBT_OK;
        }

        /* 根据协议初始化该连接的相关数据
         */
        if( wbt_module_on_conn(ev) != WBT_OK ) {
            wbt_log_add("connection close: on_conn failed\n");
            wbt_on_close(ev);
            return WBT_OK;
        }
    }

    if( wbt_module_on_recv(ev) != WBT_OK ) {
        /* 注意：一旦某一模块返回 WBT_ERROR，则后续模块将不会再执行。
         */
        wbt_log_add("connection close: on_recv failed\n");
        wbt_on_close(ev);
        return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_on_send(wbt_event_t *ev) {
    /* 数据发送已经就绪 */
    //wbt_log_debug("send data to connection %d.", ev->fd);
    
    if( wbt_module_on_send(ev) != WBT_OK ) {
        wbt_log_add("connection close: on_send failed\n");
        wbt_on_close(ev);
        return WBT_OK;
    }
    
    wbt_module_on_success(ev);

    return WBT_OK;
}

/*
wbt_status wbt_on_close(wbt_event_t *ev) {
    return WBT_OK;
}
*/

ssize_t wbt_recv(wbt_event_t *ev, void *buf, size_t len) {
    int ret;
    wbt_err_t err;
#ifdef WBT_DEBUG
    char ebuf[256];
    unsigned long e;
#endif

    wbt_set_errno(0);

    if(ev->ssl) {
        ret = SSL_read(ev->ssl, buf, len);
        if(ret <= 0) {
            err = SSL_get_error(ev->ssl, ret);
            if(err == SSL_ERROR_WANT_READ) {
                ret = -1;
                wbt_set_errno(WBT_EAGAIN);
            } else if(err == SSL_ERROR_WANT_WRITE) {
                ret = -1;
                //ev->want_write = true;
                wbt_set_errno(WBT_EAGAIN);
            } else {
#ifdef WBT_DEBUG
                e = ERR_get_error();
                while(e){
                    wbt_log_debug("OpenSSL Error: %s\n", ERR_error_string(e, ebuf));
                    e = ERR_get_error();
                }
#endif
                wbt_set_errno(WBT_ENOPROTOOPT);
            }
        }
        return (ssize_t )ret;
    }else{
        /* Call normal read/recv */
        return recv(ev->fd, buf, len, 0);
    }
}

ssize_t wbt_send(wbt_event_t *ev, void *buf, size_t len) {
    int ret;
    wbt_err_t err;
#ifdef WBT_DEBUG
    char ebuf[256];
    unsigned long e;
#endif

    wbt_set_errno(0);

    if(ev->ssl){
        //ev->want_write = false;
        ret = SSL_write(ev->ssl, buf, len);
        if(ret < 0){
            err = SSL_get_error(ev->ssl, ret);
            if(err == SSL_ERROR_WANT_READ){
                ret = -1;
                wbt_set_errno(WBT_EAGAIN);
            }else if(err == SSL_ERROR_WANT_WRITE){
                ret = -1;
                //mosq->want_write = true;
                wbt_set_errno(WBT_EAGAIN);
            }else{
#ifdef WBT_DEBUG
                e = ERR_get_error();
                while(e){
                    wbt_log_debug("OpenSSL Error: %s\n", ERR_error_string(e, ebuf));
                    e = ERR_get_error();
                }
#endif
                wbt_set_errno(WBT_ENOPROTOOPT);
            }
        }
        return (ssize_t )ret;
    }else{
        /* Call normal write/send */
        return send(ev->fd, buf, len, 0);
    }
}
