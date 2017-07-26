/* 
 * File:   wbt_mq_replication.c
 * Author: fcten
 *
 * Created on 2017年6月9日, 下午7:13
 */

#include "wbt_mq_replication.h"
#include "../bmtp2/wbt_bmtp2.h"

wbt_repl_t wbt_replication;

wbt_repl_srv_t wbt_repl_server;

wbt_status wbt_repl_on_recv(wbt_event_t *ev);
wbt_status wbt_repl_on_send(wbt_event_t *ev);

wbt_status wbt_mq_repl_init() {
    // 初始化本实例作为 master 的相关数据结构
    wbt_list_init(&wbt_replication.client_list.head);

    // 如果存在 maseter
    if(wbt_conf.master_host.len == 0) {
        return WBT_OK;
    }
    
    wbt_repl_server.addr.sin_family = AF_INET;
    wbt_repl_server.addr.sin_port = htons(wbt_conf.master_port);
#ifdef WIN32
    wbt_repl_server.addr.sin_addr.S_un.S_addr = inet_addr(wbt_stdstr(&wbt_conf.master_host));
#else
    wbt_repl_server.addr.sin_addr.s_addr = inet_addr(wbt_stdstr(&wbt_conf.master_host));
#endif
    memset(wbt_repl_server.addr.sin_zero, 0x00, 8);

    wbt_event_t tmp_ev;
            
    tmp_ev.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(tmp_ev.fd, (struct sockaddr*)&wbt_repl_server.addr, sizeof(wbt_repl_server.addr)) == -1) {
        wbt_log_add("replication: connect to %.*s:%d failed.\n",
            wbt_conf.master_host.len,
            wbt_conf.master_host.str,
            wbt_conf.master_port);
        return WBT_ERROR;
    }
    tmp_ev.timer.on_timeout = wbt_conn_close;
    tmp_ev.timer.timeout    = wbt_cur_mtime + wbt_conf.event_timeout;
    tmp_ev.on_recv = wbt_on_recv;
    tmp_ev.on_send = wbt_on_send;
    tmp_ev.events  = WBT_EV_READ | WBT_EV_ET;

    wbt_nonblocking(tmp_ev.fd);
    
    wbt_repl_server.ev = wbt_event_add(&tmp_ev);
    if( wbt_repl_server.ev == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_repl_server.ev->protocol = WBT_PROTOCOL_BMTP;
    wbt_bmtp2_on_conn(wbt_repl_server.ev);
    
    wbt_bmtp2_t *bmtp = wbt_repl_server.ev->data;
    bmtp->role = BMTP_CLIENT;
    
    wbt_bmtp2_send_conn(wbt_repl_server.ev, NULL);

    return WBT_OK;
}

wbt_status wbt_mq_repl_reset_timer();

wbt_status wbt_mq_repl_heartbeat(wbt_timer_t *timer) {
    if( wbt_repl_server.ev == NULL ) {
        return WBT_OK;
    }
    
    wbt_bmtp2_t *bmtp = wbt_repl_server.ev->data;
    
    if( bmtp->is_conn ) {
        wbt_bmtp2_send_ping(wbt_repl_server.ev);
        
        wbt_mq_repl_reset_timer();
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_repl_reset_timer() {
    static wbt_timer_t *timer = NULL;
    
    if( timer == NULL ) {
        timer = wbt_calloc(sizeof(wbt_timer_t));
        if( timer == NULL ) {
            return WBT_ERROR;
        }
    }
    
    timer->on_timeout = wbt_mq_repl_heartbeat;
    timer->timeout = wbt_cur_mtime + 30 * 1000;
    
    return wbt_timer_mod(timer);
}

wbt_status wbt_mq_repl_reconnect(wbt_timer_t *timer) {
    wbt_log_debug("replication: try to reconnect\n");
    
    wbt_event_t tmp_ev;
            
    tmp_ev.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    wbt_nonblocking(tmp_ev.fd);

    if (connect(tmp_ev.fd, (struct sockaddr*)&wbt_repl_server.addr, sizeof(wbt_repl_server.addr)) == -1) {
#ifndef WIN32
        if (wbt_socket_errno == WBT_EINPROGRESS) {
#else
        if (wbt_socket_errno == WBT_EAGAIN) {
            /* Winsock returns WSAEWOULDBLOCK (WBT_EAGAIN) */
#endif
            
        } else {
            close(tmp_ev.fd);
            
            goto again;
        }
    }
    tmp_ev.timer.on_timeout = wbt_conn_close;
    tmp_ev.timer.timeout    = wbt_cur_mtime + wbt_conf.event_timeout;
    tmp_ev.on_recv = wbt_on_recv;
    tmp_ev.on_send = wbt_on_send;
    tmp_ev.events  = WBT_EV_READ | WBT_EV_ET;

    wbt_repl_server.ev = wbt_event_add(&tmp_ev);
    if( wbt_repl_server.ev == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_repl_server.ev->protocol = WBT_PROTOCOL_BMTP;
    wbt_bmtp2_on_conn(wbt_repl_server.ev);
    
    wbt_bmtp2_t *bmtp = wbt_repl_server.ev->data;
    bmtp->role = BMTP_CLIENT;
    
    wbt_bmtp2_send_conn(wbt_repl_server.ev, NULL);
    
    return WBT_OK;

again:
    wbt_repl_server.reconnect_timer.on_timeout = wbt_mq_repl_reconnect;
    wbt_repl_server.reconnect_timer.timeout = wbt_cur_mtime + 5 * 1000;
    wbt_timer_mod(&wbt_repl_server.reconnect_timer);
    
    return WBT_OK;
}
    
wbt_status wbt_mq_repl_on_open(wbt_event_t *ev) {
    
    
    return wbt_mq_repl_reset_timer();
}

wbt_status wbt_mq_repl_on_close(wbt_event_t *ev) {
    wbt_log_debug("replication: lost connection\n");
    
    wbt_repl_server.ev = NULL;
    
    // 尝试重连服务端
    wbt_repl_server.reconnect_timer.on_timeout = wbt_mq_repl_reconnect;
    wbt_repl_server.reconnect_timer.timeout = wbt_cur_mtime + 5 * 1000;
    wbt_timer_mod(&wbt_repl_server.reconnect_timer);
    
    return WBT_OK;
}

wbt_status wbt_mq_repl_notify_all() {
    wbt_repl_cli_t *node;
    wbt_list_for_each_entry(node, wbt_repl_cli_t, &wbt_replication.client_list.head, head) {
        wbt_mq_repl_notify(node);
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_repl_notify(wbt_repl_cli_t *cli) {
    wbt_bmtp2_t *bmtp = cli->subscriber->ev->data;
    
    if( bmtp->window == 0 ) {
        return WBT_OK;
    }
    
    // TODO 根据 cli->msg_id 发送同步数据
    
    return WBT_OK;
}

wbt_repl_cli_t * wbt_mq_repl_client_new(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return NULL;
    }
    
    wbt_repl_cli_t *cli = wbt_calloc(sizeof(wbt_repl_cli_t));
    if( cli ) {
        cli->subscriber = subscriber;
        wbt_list_add_tail(&cli->head, &wbt_replication.client_list.head);
    }
    
    return cli;
}

wbt_repl_cli_t * wbt_mq_repl_client_get(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    
    wbt_repl_cli_t *node, *cli = NULL;
    wbt_list_for_each_entry(node, wbt_repl_cli_t, &wbt_replication.client_list.head, head) {
        if( node->subscriber == subscriber ) {
            cli = node;
            break;
        }
    }
    
    return cli;
}