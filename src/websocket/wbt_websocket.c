 /* 
 * File:   wbt_websocket.c
 * Author: Fcten
 *
 * Created on 2017年01月17日, 下午4:08
 */

#include "../common/wbt_module.h"
#include "../common/wbt_memory.h"
#include "../common/wbt_log.h"
#include "../common/wbt_time.h"
#include "../common/wbt_config.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_ssl.h"
#include "../common/wbt_string.h"
#include "../common/wbt_file.h"
#include "../common/wbt_gzip.h"
#include "../common/wbt_base64.h"
#include "wbt_websocket.h"
#include "http/wbt_http.h"

#ifdef WITH_WEBSOCKET

extern wbt_atomic_t wbt_wating_to_exit;

const wbt_str_t handshake = wbt_string(
    "HTTP/1.1 101 Switching Protocols" CRLF
    "Upgrade: websocket" CRLF
    "Connection: Upgrade" CRLF
    "Sec-WebSocket-Accept: 1234567890123456789012345678" CRLF CRLF
);

enum {
    STATE_CONNECTED,
    STATE_RECV_HEADER,
    STATE_RECV_PAYLOAD_LENGTH,
    STATE_RECV_MASK,
    STATE_RECV_PAYLOAD
};

wbt_module_t wbt_module_websocket = {
    wbt_string("websocket"),
    wbt_websocket_init,
    wbt_websocket_exit,
    NULL, // websocket 连接需要由 http 连接升级而来
    wbt_websocket_on_recv,
    wbt_websocket_on_send,
    wbt_websocket_on_close
};

wbt_status wbt_websocket_init() {
    return WBT_OK;
}

wbt_status wbt_websocket_exit() {
    return WBT_OK;
}

wbt_status wbt_websocket_on_conn( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }
    
    ev->data = wbt_calloc( sizeof(wbt_websocket_t) );
    if( ev->data == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_websocket_t *ws = ev->data;

    wbt_list_init(&ws->send_queue.head);
    
    return WBT_OK;
}

wbt_status wbt_websocket_on_send( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }

    wbt_websocket_t *ws = ev->data;

    while (!wbt_list_empty(&ws->send_queue.head)) {
        wbt_websocket_msg_t *msg_node = wbt_list_first_entry(&ws->send_queue.head, wbt_websocket_msg_t, head);
        int ret = wbt_send(ev, (char *)msg_node->msg + msg_node->offset, msg_node->len - msg_node->offset);
        if (ret <= 0) {
            if( wbt_socket_errno == WBT_EAGAIN ) {
                return WBT_OK;
            } else {
                wbt_log_add("wbt_send failed: %d\n", wbt_socket_errno );
                return WBT_ERROR;
            }
        } else {
            if (ret + msg_node->offset >= msg_node->len) {
                wbt_list_del(&msg_node->head);

                wbt_free(msg_node->msg);
                wbt_free(msg_node);
            } else {
                msg_node->offset += ret;
            }
        }
    }
    
    if( ws->is_exit ) {
        wbt_on_close(ev);
    } else {
        ev->events &= ~WBT_EV_WRITE;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_websocket_on_close( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }

    return WBT_OK;
}

wbt_status wbt_websocket_on_recv( wbt_event_t *ev ) {
    if( ev->protocol == WBT_PROTOCOL_HTTP ) {
        /* websocket 协议由 http 协议升级而来
         */
        wbt_http_t * http = ev->data;

        if( http->status != STATUS_404 || http->method != METHOD_GET ) {
            return WBT_OK;
        }

        wbt_str_t uri = wbt_string("/mq/ws/");

        wbt_str_t http_uri;
        wbt_offset_to_str(http->uri, http_uri, ev->buff);

        if( wbt_strcmp( &http_uri, &uri ) != 0 ) {
            return WBT_OK;
        }

        wbt_http_header_t * header;
        wbt_str_t upgrade, connection, sec_websocket_key, sec_websocket_version;
        header = http->headers;
        while( header != NULL ) {
            switch( header->key ) {
                case HEADER_SEC_WEBSOCKET_KEY:
                    wbt_offset_to_str(header->value.o, sec_websocket_key, ev->buff);
                    break;
                case HEADER_SEC_WEBSOCKET_VERSION:
                    wbt_offset_to_str(header->value.o, sec_websocket_version, ev->buff);
                    break;
                case HEADER_UPGRADE:
                    wbt_offset_to_str(header->value.o, upgrade, ev->buff);
                    break;
                case HEADER_CONNECTION:
                    wbt_offset_to_str(header->value.o, connection, ev->buff);
                    break;
            }

            header = header->next;
        }
        
        wbt_str_t s1 = wbt_string("Upgrade");
        wbt_str_t s2  = wbt_string("websocket");
        wbt_str_t s3  = wbt_string("13");
        
        if( wbt_strcmp( &connection, &s1 ) != 0 ) {
            http->status = STATUS_400;
            return WBT_OK;
        }
        if( wbt_strcmp( &upgrade, &s2 ) != 0 ) {
            http->status = STATUS_400;
            return WBT_OK;
        }
        if( wbt_strcmp( &sec_websocket_version, &s3 ) != 0 ) {
            http->status = STATUS_400;
            wbt_http_set_header(http, HEADER_SEC_WEBSOCKET_VERSION, &s3);
            return WBT_OK;
        }
        if( sec_websocket_key.len == 0 ) {
            http->status = STATUS_400;
            return WBT_OK;
        }
        
        // 切换为 websocket
        wbt_http_on_close(ev);
        ev->protocol = WBT_PROTOCOL_WEBSOCKET;
        wbt_websocket_on_conn(ev);
        
        // 生成响应
        wbt_str_t key  = wbt_string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        unsigned char md[SHA_DIGEST_LENGTH];
        SHA_CTX c;
        SHA1_Init(&c);
        SHA1_Update(&c, sec_websocket_key.str, sec_websocket_key.len);
        SHA1_Update(&c, key.str, key.len);
        SHA1_Final(md, &c);
        //OPENSSL_cleanse(&c, sizeof(c));
        
        wbt_str_t buf;
        buf.len = handshake.len;
        buf.str = wbt_malloc(buf.len);
        wbt_memcpy(buf.str, handshake.str, handshake.len);
        
        wbt_str_t src, dst;
        src.len = SHA_DIGEST_LENGTH;
        src.str = md;
        dst.len = 28;
        dst.str = buf.str + buf.len - 32; // 28 + 4
        wbt_base64_encode(&dst, &src);
        
        return wbt_websocket_send(ev, buf.str, buf.len);
    } else if( ev->protocol == WBT_PROTOCOL_WEBSOCKET ) {
        wbt_log_debug("ws: recv data\n");
    }

    return WBT_OK;
}

wbt_status wbt_websocket_send(wbt_event_t *ev, char *buf, int len) {
    wbt_websocket_t *ws = ev->data;

    wbt_websocket_msg_t *msg_node = (wbt_websocket_msg_t *)wbt_calloc(sizeof(wbt_websocket_msg_t));
    if (!msg_node) {
        return WBT_ERROR;
    }

    msg_node->len = len;
    msg_node->msg = buf;

    wbt_list_add_tail(&msg_node->head, &ws->send_queue.head);
    wbt_log_debug( "ws: add to send queue\n" );

    ev->events |= WBT_EV_WRITE;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

#endif