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
    STATE_RECV_MASK_KEY,
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
        wbt_str_t upgrade, connection, sec_websocket_version, sec_websocket_key = wbt_null_string;
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
                default:
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
        src.str = (char *)md;
        dst.len = 28;
        dst.str = buf.str + buf.len - 32; // 28 + 4
        wbt_base64_encode(&dst, &src);
        
        return wbt_websocket_send(ev, (unsigned char *)buf.str, buf.len);
    } else if( ev->protocol == WBT_PROTOCOL_WEBSOCKET ) {
        //wbt_log_debug("ws: recv data\n");
        
        wbt_websocket_t *ws = ev->data;
    
        if( ws->is_exit ) {
            return WBT_OK;
        }

        int msg_offset = 0;
        unsigned char c;

        while(!ws->is_exit) {
            switch(ws->state) {
                case STATE_CONNECTED:
                    ws->payload = NULL;
                    ws->payload_length = 0;
                    ws->state = STATE_RECV_HEADER;
                    break;
                case STATE_RECV_HEADER:
                    msg_offset = ws->recv_offset;

                    if( ws->recv_offset + 2 > ev->buff_len ) {
                        goto waiting;
                    }

                    c = ((unsigned char *)ev->buff)[ws->recv_offset ++];

                    switch(c & 0xF0) {
                        case 0x80:
                            ws->fin = 1;
                            break;
                        case 0x00:
                            ws->fin = 0;
                            break;
                        default:
                            return WBT_ERROR;
                    }
                    
                    ws->opcode = c & 0x0F;
                    switch(ws->opcode) {
                        case 0x0:
                        case 0x1:
                        case 0x2:
                        case 0x8:
                        case 0x9:
                        case 0xA:
                            break;
                        default:
                            return WBT_ERROR;
                    }
                    
                    c = ((unsigned char *)ev->buff)[ws->recv_offset ++];
                    
                    ws->mask = c >> 7;
                    ws->payload_length = c & 0x7F;
                    
                    if( ws->mask != 1 ) {
                        return WBT_ERROR;
                    }
                    
                    ws->state = STATE_RECV_PAYLOAD_LENGTH;
                    break;
                case STATE_RECV_PAYLOAD_LENGTH:
                    if( ws->payload_length <= 125 ) {
                        ws->state = STATE_RECV_MASK_KEY;
                    } else if( ws->payload_length == 126 ) {
                        if( ws->recv_offset + 2 > ev->buff_len ) {
                            goto waiting;
                        }

                        ws->payload_length  = ((unsigned char *)ev->buff)[ws->recv_offset ++] << 8;
                        ws->payload_length += ((unsigned char *)ev->buff)[ws->recv_offset ++];
                        
                        ws->state = STATE_RECV_MASK_KEY;
                    } else if( ws->payload_length == 127 ) {
                        if( ws->recv_offset + 8 > ev->buff_len ) {
                            goto waiting;
                        }

                        ws->payload_length = *((unsigned long long int *)ev->buff);
                        ws->recv_offset += 8;
                        
                        ws->state = STATE_RECV_MASK_KEY;
                    } else {
                        return WBT_ERROR;
                    }
                    break;
                case STATE_RECV_MASK_KEY:
                    if( ws->recv_offset + 4 > ev->buff_len ) {
                        goto waiting;
                    }

                    ws->mask_key[0] = ((unsigned char *)ev->buff)[ws->recv_offset ++];
                    ws->mask_key[1] = ((unsigned char *)ev->buff)[ws->recv_offset ++];
                    ws->mask_key[2] = ((unsigned char *)ev->buff)[ws->recv_offset ++];
                    ws->mask_key[3] = ((unsigned char *)ev->buff)[ws->recv_offset ++];
                    
                    ws->state = STATE_RECV_PAYLOAD;
                    break;
                case STATE_RECV_PAYLOAD:
                    if( ws->recv_offset + ws->payload_length > ev->buff_len ) {
                        goto waiting;
                    }

                    ws->payload = (unsigned char *)ev->buff + ws->recv_offset;
                    ws->recv_offset += ws->payload_length;

                    // 解码
                    for(int i = 0;i < ws->payload_length; i++) {
                        ws->payload[i] ^= ws->mask_key[i%4];
                    }
                    

                    wbt_websocket_send_msg(ev, ws->payload, ws->payload_length);
                    
                    ws->state = STATE_CONNECTED;
                    break;
                default:
                    return WBT_ERROR;
            }
        }

    waiting:

        if( msg_offset > 0 ) {
            /* 删除已经处理完毕的消息
             */
            if( ev->buff_len == msg_offset ) {
                wbt_free(ev->buff);
                ev->buff = NULL;
                ev->buff_len = 0;
                ws->recv_offset = 0;
            } else if( ev->buff_len > msg_offset ) {
                wbt_memmove(ev->buff, (unsigned char *)ev->buff + msg_offset, ev->buff_len - msg_offset);
                ev->buff_len -= msg_offset;
                ws->recv_offset -= msg_offset;
            } else {
                wbt_log_add("ws error: unexpected error\n");
                return WBT_ERROR;
            }
        } else if( msg_offset == 0 &&
                ws->recv_offset + ws->payload_length > WBT_MAX_PROTO_BUF_LEN ) {
            /* 消息长度超过限制
             */
            wbt_log_add("ws error: message length exceeds limit\n");
            return WBT_ERROR;
        }

        ev->events |= WBT_EV_READ;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;
        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }

        return WBT_OK;

    }

    return WBT_OK;
}

wbt_status wbt_websocket_send_msg(wbt_event_t *ev, unsigned char *data, int len) {
    if( len > 64 * 1024 - 4 ) {
        len = 64 * 1024 - 4;
    }

    if( len <= 125 ) {
        unsigned char *buf = wbt_malloc(len+2);
        buf[0] = 0x82;
        buf[1] = len;
        wbt_memcpy(buf+2, data, len);
        return wbt_websocket_send(ev, buf, len+2);
    } else {
        unsigned char *buf = wbt_malloc(len+4);
        buf[0] = 0x82;
        buf[1] = 126;
        buf[2] = len >> 8;
        buf[3] = len;
        wbt_memcpy(buf+4, data, len);
        return wbt_websocket_send(ev, buf, len+4);
    }
}

wbt_status wbt_websocket_send(wbt_event_t *ev, unsigned char *buf, int len) {
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