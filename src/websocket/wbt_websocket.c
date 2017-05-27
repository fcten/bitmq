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
#include "../http/wbt_http.h"
#include "../bmtp/wbt_bmtp.h"
#include "wbt_websocket.h"

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

#define BEGIN_BMTP_CTX if(1){\
    wbt_websocket_t *ws = ev->data;\
    ev->data = &ws->bmtp;\
    ev->protocol = WBT_PROTOCOL_BMTP;
#define ENDOF_BMTP_CTX \
    ev->data = ws;\
    ev->protocol = WBT_PROTOCOL_WEBSOCKET;}

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

    BEGIN_BMTP_CTX;
    wbt_bmtp_on_conn(ev);
    ENDOF_BMTP_CTX;
    
    return WBT_OK;
}

wbt_status wbt_websocket_on_send( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }

    BEGIN_BMTP_CTX;
    wbt_bmtp_on_send(ev);
    ENDOF_BMTP_CTX;

    return WBT_OK;
}

wbt_status wbt_websocket_on_close( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }

    BEGIN_BMTP_CTX;
    wbt_bmtp_on_close(ev);
    ENDOF_BMTP_CTX;
    
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
        
        /* 注意，这里 buf[0] = 'H'，即 0x48。在 BMTP 中，该数据帧会被当作 PUBACK 处理。
         * 目前 PUBACK 不经任何处理直接发送，所以暂时不会产生问题。
         */
        BEGIN_BMTP_CTX;
        wbt_bmtp_send(ev, buf.str, buf.len);
        ENDOF_BMTP_CTX;
    } else if( ev->protocol == WBT_PROTOCOL_WEBSOCKET ) {
        //wbt_log_debug("ws: recv data\n");
        
        wbt_websocket_t *ws = ev->data;
    
        if( ev->is_exit ) {
            return WBT_OK;
        }

        unsigned int msg_offset = 0;
        unsigned char c;

        while(!ev->is_exit) {
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
                    ws->recv_offset += (unsigned int)ws->payload_length;

                    // 解码
                    for(int i = 0;i < ws->payload_length; i++) {
                        ws->payload[i] ^= ws->mask_key[i%4];
                    }
                    
                    if( ws->fin == 1 && ws->opcode > 0 ) {
                        // 未分片消息
                        switch(ws->opcode) {
                            case 0x1: // 文本帧
                            case 0x2: // 二进制帧
                                //wbt_websocket_send_msg(ev, ws->payload, (unsigned int)ws->payload_length);
                                BEGIN_BMTP_CTX;
                                void *tmp_buff = ev->buff;
                                unsigned int tmp_buff_len = ev->buff_len;
                                ev->buff = ws->payload;
                                ev->buff_len = ws->payload_length;
                                if( wbt_bmtp_on_recv(ev) != WBT_OK || ev->buff_len > 0 ) {
                                    // error
                                    wbt_bmtp_send_disconn(ev);
                                }
                                ev->buff = tmp_buff;
                                ev->buff_len = tmp_buff_len;
                                ENDOF_BMTP_CTX;
                                break;
                            case 0x8: // 关闭帧
                                BEGIN_BMTP_CTX;
                                wbt_bmtp_send_disconn(ev);
                                ENDOF_BMTP_CTX;
                                break;
                            case 0x9: // ping
                            case 0xA: // pong
                                // do nothing
                                break;
                            default:
                                return WBT_ERROR;
                        }
                    } else if( ws->fin == 1 && ws->opcode == 0 ) {
                        // 分片消息的最后一帧
                    } else { // ws->fin == 0
                        switch(ws->opcode) {
                            case 0x0: // 继续帧
                                break;
                            case 0x1: // 文本帧
                            case 0x2: // 二进制帧
                                break;
                            default:
                                return WBT_ERROR;
                        }
                    }

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
/*
wbt_status wbt_websocket_send_frame_header(wbt_event_t *ev, int len) {
    if( len > 64 * 1024 - 4 ) {
        len = 64 * 1024 - 4;
    }

    if( len <= 125 ) {
        unsigned char buf[] = {0x82, len};
        return wbt_websocket_send(ev, buf, sizeof(buf));
    } else {
        unsigned char buf[] = {0x82, 126, len >> 8, len};
        return wbt_websocket_send(ev, buf, sizeof(buf));
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
*/
#endif