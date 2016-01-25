/* 
 * File:   wbt_connection.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午3:57
 */

#include "wbt_connection.h"
#include "wbt_event.h"
#include "wbt_string.h"
#include "wbt_log.h"
#include "wbt_heap.h"
#include "wbt_module.h"
#include "wbt_file.h"
#include "wbt_config.h"
#include "wbt_time.h"
#include "wbt_ssl.h"

wbt_atomic_t wbt_connection_count = 0;

wbt_status wbt_setnonblocking(int sock);

wbt_module_t wbt_module_conn = {
    wbt_string("connection"),
    wbt_conn_init,
    wbt_conn_cleanup
};

int listen_fd;

wbt_status wbt_conn_init() {
    // TODO linux 3.9 以上内核支持 REUSE_PORT，可以优化多核性能
    
    /* 初始化用于监听消息的 Socket 句柄 */
    char * listen_fd_env = getenv("WBT_LISTEN_FD");
    if(listen_fd_env == NULL) {
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

        /* 在重启程序以及进行热更新时，避免 TIME_WAIT 和 CLOSE_WAIT 状态的连接导致 bind 失败 */
        int on = 1; 
        if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0) {  
            wbt_log_add("set SO_REUSEADDR failed\n");  

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
    } else {
        listen_fd = atoi(listen_fd_env);
    }
    
    wbt_log_add("listen fd: %d\n", listen_fd);

    return WBT_OK;
}

wbt_status wbt_conn_cleanup() {
    close(listen_fd);
    
    return WBT_OK;
}

wbt_status wbt_conn_close(wbt_event_t *ev) {
    //wbt_log_debug("connection %d close.",ev->fd);
    
    if( wbt_module_on_close(ev) != WBT_OK ) {
        // 似乎并不能做什么
    }

    close(ev->fd);
    ev->fd = -1;        /* close 之后 fd 会自动从 epoll 中删除 */
    wbt_event_del(ev);
    
    wbt_connection_count --;

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
        tmp_ev.on_timeout = wbt_conn_close;
        tmp_ev.on_recv = wbt_on_recv;
        tmp_ev.on_send = NULL;
        tmp_ev.events = EPOLLIN | EPOLLET;
        tmp_ev.fd = conn_sock;
        tmp_ev.timeout = wbt_cur_mtime + wbt_conf.event_timeout;

        if((p_ev = wbt_event_add(&tmp_ev)) == NULL) {
            return WBT_ERROR;
        }
        
        wbt_connection_count ++;
        
        if( wbt_module_on_conn(p_ev) != WBT_OK ) {
            wbt_conn_close(p_ev);
            return WBT_OK;
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
    //wbt_log_debug("recv data of connection %d.", fd);
    
    int nread;
    int bReadOk = 0;

    while( ev->buff.len <= 40960 ) { /* 限制数据包长度 */
        /* TODO realloc 意味着潜在的内存拷贝行为，目前的代码在接收大请求时效率很低 */
        if( wbt_realloc(&ev->buff, ev->buff.len + 4096) != WBT_OK ) {
            /* 内存不足 */
            wbt_conn_close(ev);

            return WBT_OK;
        }
        nread = wbt_ssl_read(ev, ev->buff.ptr + ev->buff.len - 4096, 4096);
        if(nread < 0) {
            if(errno == EAGAIN) {
                // 当前缓冲区已无数据可读
                bReadOk = 1;
                
                /* 去除多余的缓冲区 */
                wbt_realloc(&ev->buff, ev->buff.len - 4096);
                
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

       if ( nread > 0) {
            /* 去除多余的缓冲区 */
            wbt_realloc(&ev->buff, ev->buff.len - 4096 + nread);
            
           continue;   // 需要再次读取
       } else {
           // 安全读完
           bReadOk = 1;
           break; // 退出while(1),表示已经全部读完数据
       }
    }

    if( !bReadOk ) {
        /* 读取出错，或者客户端主动断开了连接 */
        wbt_conn_close(ev);
        return WBT_OK;
    }

    /* 自定义的处理回调函数，根据 URI 返回自定义响应结果 */
    if( wbt_module_on_recv(ev) != WBT_OK ) {
        /* 严重的错误，直接断开连接 */
        /* 注意：一旦某一模块返回 WBT_ERROR，则后续模块将不会再执行。 */
        wbt_conn_close(ev);
        return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_on_send(wbt_event_t *ev) {
    /* 数据发送已经就绪 */
    //wbt_log_debug("send data to connection %d.", ev->fd);
    
    if( wbt_module_on_send(ev) != WBT_OK ) {
        wbt_conn_close(ev);
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
