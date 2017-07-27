/* 
 * File:   wbt_bmtp2.c
 * Author: fcten
 *
 * Created on 2017年6月19日, 下午9:31
 */

#include "wbt_bmtp2.h"
#include "../mq/wbt_mq_replication.h"

wbt_module_t wbt_module_bmtp2 = {
    wbt_string("bmtp2"),
    wbt_bmtp2_init,
    wbt_bmtp2_exit,
    wbt_bmtp2_on_conn,
    wbt_bmtp2_on_recv,
    wbt_bmtp2_on_send,
    wbt_bmtp2_on_close,
    NULL,
    wbt_bmtp2_on_handler
};

wbt_bmtp2_cmd_t wbt_bmtp2_cmd_table[] = {
    { 0, wbt_string("RSV"),     NULL                 },
    { 1, wbt_string("CONN"),    wbt_bmtp2_on_connect },
    { 2, wbt_string("CONNACK"), wbt_bmtp2_on_connack },
    { 3, wbt_string("PUB"),     wbt_bmtp2_on_pub     },
    { 4, wbt_string("PUBACK"),  wbt_bmtp2_on_puback  },
    { 5, wbt_string("SUB"),     wbt_bmtp2_on_sub     },
    { 6, wbt_string("SUBACK"),  wbt_bmtp2_on_suback  },
    { 7, wbt_string("PING"),    wbt_bmtp2_on_ping    },
    { 8, wbt_string("PINGACK"), wbt_bmtp2_on_pingack },
    { 9, wbt_string("DISCONN"), wbt_bmtp2_on_disconn },
    {10, wbt_string("WINDOW"),  wbt_bmtp2_on_window  },
    {11, wbt_string("RSV"),     NULL                 },
    {12, wbt_string("RSV"),     NULL                 },
    {13, wbt_string("RSV"),     NULL                 },
    {14, wbt_string("RSV"),     NULL                 },
    {15, wbt_string("RSV"),     NULL                 },
    {16, wbt_string("SYNC"),    wbt_bmtp2_on_sync    }
};

enum {
    STATE_START = 0,
    STATE_KEY,
    STATE_SPECIAL_HEADER,
    STATE_KEY_EXT1,
    STATE_KEY_EXT2,
    STATE_KEY_EXT3,
    STATE_KEY_EXT4,
    STATE_VALUE,
    STATE_VALUE_BOOL,
    STATE_VALUE_VARINT,
    STATE_VALUE_VARINT_EXT1,
    STATE_VALUE_VARINT_EXT2,
    STATE_VALUE_VARINT_EXT3,
    STATE_VALUE_VARINT_EXT4,
    STATE_VALUE_VARINT_EXT5,
    STATE_VALUE_VARINT_EXT6,
    STATE_VALUE_VARINT_EXT7,
    STATE_VALUE_VARINT_EXT8,
    STATE_VALUE_VARINT_EXT9,
    STATE_VALUE_STRING,
    STATE_VALUE_64BIT,
    STATE_PAYLOAD_LENGTH,
    STATE_PAYLOAD_LENGTH_EXT1,
    STATE_PAYLOAD_LENGTH_EXT2,
    STATE_PAYLOAD_LENGTH_EXT3,
    STATE_PAYLOAD_LENGTH_EXT4,
    STATE_PAYLOAD,
    STATE_END
};

wbt_status wbt_bmtp2_init() {
    return WBT_OK;
}

wbt_status wbt_bmtp2_exit() {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_conn(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    ev->data = ev->data ? ev->data : wbt_calloc( sizeof(wbt_bmtp2_t) );
    if( ev->data == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_t *bmtp = ev->data;
    
    bmtp->state  = STATE_START;
    bmtp->window = 64 * 1024;
    bmtp->role   = BMTP_SERVER;
    
    wbt_list_init(&bmtp->send_list.head);
    
    wbt_log_add("BMTP connect: %d\n", ev->fd);
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_recv(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP || ev->is_exit ) {
        return WBT_OK;
    }

    wbt_bmtp2_t *bmtp = ev->data;
    unsigned char c;
    
    bmtp->msg_offset = 0;
    
    while(!ev->is_exit) {
        switch(bmtp->state) {
            case STATE_START:
                bmtp->msg_offset = bmtp->recv_offset;
                
                bmtp->state = STATE_KEY;
                break;
            case STATE_KEY:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }

                c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];

                bmtp->op_code = c >> 3;
                bmtp->op_type = c;

                bmtp->state = STATE_SPECIAL_HEADER;
                break;
            case STATE_SPECIAL_HEADER:
                if( bmtp->op_code == OP_CONN ) {
                    if( bmtp->recv_offset + 4 > ev->buff_len ) {
                        return WBT_OK;
                    }

                    c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                    if( c != 'B' ) {
                        return WBT_ERROR;
                    }

                    c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                    if( c != 'M' ) {
                        return WBT_ERROR;
                    }

                    c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                    if( c != 'T' ) {
                        return WBT_ERROR;
                    }

                    c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                    if( c != 'P' ) {
                        return WBT_ERROR;
                    }
                }
                
                if( bmtp->op_code & 0x10 ) {
                    bmtp->op_code &= 0x0F;
                    bmtp->state = STATE_KEY_EXT1;
                } else {
                    bmtp->state = STATE_VALUE;
                }
                break;
            case STATE_KEY_EXT1:
            case STATE_KEY_EXT2:
            case STATE_KEY_EXT3:
            case STATE_KEY_EXT4:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }

                c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];

                bmtp->op_code += ( c & 0x7F ) << ( 4 + ( bmtp->state - STATE_KEY_EXT1 ) * 7 );
                
                if( c & 0x80 ) {
                    if( bmtp->state == STATE_KEY_EXT4 ) {
                        // key 最长不能超过 32 位
                        return WBT_ERROR;
                    } else {
                        bmtp->state += 1;
                    }
                } else {
                    bmtp->state = STATE_VALUE;
                }
                break;
            case STATE_VALUE:
                switch( bmtp->op_type ) {
                    case TYPE_BOOL:   bmtp->state = STATE_VALUE_BOOL;   break;
                    case TYPE_VARINT: bmtp->state = STATE_VALUE_VARINT; break;
                    case TYPE_64BIT:  bmtp->state = STATE_VALUE_64BIT;  break;
                    case TYPE_STRING: bmtp->state = STATE_VALUE_VARINT; break;
                    default:          return WBT_ERROR;
                }
                break;
            case STATE_VALUE_BOOL:
                bmtp->op_value.l = 1;
                
                bmtp->state = STATE_PAYLOAD_LENGTH;
                break;
            case STATE_VALUE_64BIT:
                if( bmtp->recv_offset + 8 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                // 由于不同硬件环境下存在字节序的区别，这里 BitMQ 固定使用大端字节序
                int i;
                bmtp->op_value.l = 0;
                for(i=0 ; i<8 ; i++) {
                    bmtp->op_value.l <<= 8;
                    bmtp->op_value.l += ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                }

                bmtp->state = STATE_PAYLOAD_LENGTH;
                break;
            case STATE_VALUE_VARINT:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                
                bmtp->op_value.l = ( c & 0x7F );
                
                if( c & 0x80 ) {
                    bmtp->state = STATE_VALUE_VARINT_EXT1;
                } else {
                    if( bmtp->op_type == TYPE_STRING ) {
                        bmtp->state = STATE_VALUE_STRING;
                    } else {
                        bmtp->state = STATE_PAYLOAD_LENGTH;
                    }
                }
                break;
            case STATE_VALUE_VARINT_EXT1:
            case STATE_VALUE_VARINT_EXT2:
            case STATE_VALUE_VARINT_EXT3:
            case STATE_VALUE_VARINT_EXT4:
            case STATE_VALUE_VARINT_EXT5:
            case STATE_VALUE_VARINT_EXT6:
            case STATE_VALUE_VARINT_EXT7:
            case STATE_VALUE_VARINT_EXT8:
            case STATE_VALUE_VARINT_EXT9:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                
                bmtp->op_value.l += ( (wbt_mq_id)(c & 0x7F) ) << ( ( bmtp->state - STATE_VALUE_VARINT ) * 7 );
                
                if( c & 0x80 ) {
                    if( bmtp->state == STATE_VALUE_VARINT_EXT9 ) {
                        return WBT_ERROR;
                    } else {
                        bmtp->state += 1;
                    }
                } else {
                    if( bmtp->op_type == TYPE_STRING ) {
                        bmtp->state = STATE_VALUE_STRING;
                    } else {
                        bmtp->state = STATE_PAYLOAD_LENGTH;
                    }
                }
                break;
            case STATE_VALUE_STRING:
                if( bmtp->recv_offset + bmtp->op_value.l > ev->buff_len ) {
                    return WBT_OK;
                }
                
                bmtp->op_value.s = (unsigned char *)(ev->buff + bmtp->recv_offset );
                
                bmtp->recv_offset += bmtp->op_value.l;

                bmtp->state = STATE_PAYLOAD_LENGTH;
                break;
            case STATE_PAYLOAD_LENGTH:
                if( bmtp->op_code != OP_PUB ) {
                    bmtp->state = STATE_END;
                    break;
                }

                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                
                bmtp->payload_length = ( c & 0x7F );                

                if( c & 0x80 ) {
                    bmtp->state = STATE_PAYLOAD_LENGTH_EXT1;
                } else {
                    bmtp->state = STATE_PAYLOAD;
                }
                break;
            case STATE_PAYLOAD_LENGTH_EXT1:
            case STATE_PAYLOAD_LENGTH_EXT2:
            case STATE_PAYLOAD_LENGTH_EXT3:
            case STATE_PAYLOAD_LENGTH_EXT4:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                c = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                
                bmtp->payload_length += ( c & 0x7F ) << ( ( bmtp->state - STATE_PAYLOAD_LENGTH ) * 7 );                

                if( c & 0x80 ) {
                    if( bmtp->state == STATE_PAYLOAD_LENGTH_EXT4 ) {
                        return WBT_ERROR;
                    } else {
                        bmtp->state += 1;
                    }
                } else {
                    bmtp->state = STATE_PAYLOAD;
                }
                break;
            case STATE_PAYLOAD:
                if( bmtp->recv_offset + bmtp->payload_length > ev->buff_len ) {
                    return WBT_OK;
                }
                
                bmtp->payload = (unsigned char *)(ev->buff + bmtp->recv_offset );
                
                bmtp->recv_offset += bmtp->payload_length;

                bmtp->state = STATE_END;
                break;
            case STATE_END:
                if( bmtp->op_code >= OP_MAX ) {
                    // 指令不存在
                    return WBT_ERROR;
                }
                
                if( wbt_bmtp2_cmd_table[bmtp->op_code].on_proc == NULL ) {
                    // 指令不存在
                    return WBT_ERROR;
                }
                
                if( bmtp->role == BMTP_SERVER && !bmtp->is_conn && bmtp->op_code != OP_CONN ) {
                    // 连接建立后的第一条指令必须为 OP_CONN
                    return WBT_ERROR;
                } else if( bmtp->role == BMTP_CLIENT && !bmtp->is_conn && bmtp->op_code != OP_CONNACK ) {
                    // 连接建立后的第一条指令必须为 OP_CONN
                    return WBT_ERROR;
                }
                
                if( wbt_bmtp2_cmd_table[bmtp->op_code].on_proc(ev) == WBT_OK ) {
                    bmtp->state = STATE_START;
                } else {
                    return WBT_ERROR;
                }
                break;
            default:
                return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_bmtp2_param_parser(wbt_event_t *ev, wbt_status (*callback)(wbt_event_t *ev, wbt_bmtp2_param_t *p)) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->op_type != TYPE_STRING ) {
        return WBT_OK;
    }
    
    int state = STATE_START;
    int offset = 0;
    wbt_bmtp2_param_t param;
    unsigned char *buf = bmtp->op_value.s;
    unsigned char c;
    
    while( 1 ) {
        switch(state) {
            case STATE_START:
                wbt_memset(&param, 0, sizeof(param));
                
                state = STATE_KEY;
            case STATE_KEY:
                if( offset + 1 > bmtp->op_value.l ) {
                    goto end;
                }
                
                c = buf[offset++];
                
                param.key = ( c & 0x7F ) >> 3;
                param.key_type = c;
                
                if( c & 0x80 ) {
                    state = STATE_KEY_EXT1;
                } else {
                    state = STATE_VALUE;
                }
                break;
            case STATE_KEY_EXT1:
            case STATE_KEY_EXT2:
            case STATE_KEY_EXT3:
            case STATE_KEY_EXT4:
                if( offset + 1 > bmtp->op_value.l ) {
                    goto end;
                }

                c = buf[offset++];
                
                param.key += ( c & 0x7F ) << ( 4 + 7 * ( state - STATE_KEY_EXT1 ) );
                
                if( c & 0x80 ) {
                    if( state == STATE_KEY_EXT4 ) {
                        return WBT_ERROR;
                    } else {
                        state ++;
                    }
                } else {
                    state = STATE_VALUE;
                }
                break;
            case STATE_VALUE:
                switch( param.key_type ) {
                    case TYPE_BOOL:   state = STATE_VALUE_BOOL;   break;
                    case TYPE_VARINT: state = STATE_VALUE_VARINT; break;
                    case TYPE_64BIT:  state = STATE_VALUE_64BIT;  break;
                    case TYPE_STRING: state = STATE_VALUE_VARINT; break;
                    default:          return WBT_ERROR;
                }
                break;
            case STATE_VALUE_BOOL:
                param.value.l = 1;
                
                state = STATE_END;
                break;
            case STATE_VALUE_VARINT:
                if( offset + 1 > bmtp->op_value.l ) {
                    goto end;
                }

                c = buf[offset++];
                
                param.value.l = ( c & 0x7F );
                
                if( c & 0x80 ) {
                    state = STATE_VALUE_VARINT_EXT1;
                } else {
                    state = STATE_END;
                }
                break;
            case STATE_VALUE_VARINT_EXT1:
            case STATE_VALUE_VARINT_EXT2:
            case STATE_VALUE_VARINT_EXT3:
            case STATE_VALUE_VARINT_EXT4:
            case STATE_VALUE_VARINT_EXT5:
            case STATE_VALUE_VARINT_EXT6:
            case STATE_VALUE_VARINT_EXT7:
            case STATE_VALUE_VARINT_EXT8:
            case STATE_VALUE_VARINT_EXT9:
                if( offset + 1 > bmtp->op_value.l ) {
                    goto end;
                }

                c = buf[offset++];
                
                param.value.l += ( (wbt_mq_id)(c & 0x7F) ) << ( ( state - STATE_VALUE_VARINT ) * 7 );
                
                if( c & 0x80 ) {
                    if( state == STATE_VALUE_VARINT_EXT9 ) {
                        return WBT_ERROR;
                    } else {
                        state += 1;
                    }
                } else {
                    if( param.key_type == TYPE_STRING ) {
                        state = STATE_VALUE_STRING;
                    } else {
                        state = STATE_END;
                    }
                }
                break;
            case STATE_VALUE_STRING:
                if( offset + param.value.l > bmtp->op_value.l ) {
                    goto end;
                }

                param.value.s = buf + offset;
                
                offset += param.value.l;

                state = STATE_END;
                break;
            case STATE_VALUE_64BIT:
                if( offset + 8 > bmtp->op_value.l ) {
                    goto end;
                }

                // 由于不同硬件环境下存在字节序的区别，这里 BitMQ 固定使用大端字节序
                int i;
                param.value.l = 0;
                for(i=0 ; i<8 ; i++) {
                    param.value.l <<= 8;
                    param.value.l += buf[offset++];
                }

                state = STATE_END;
                break;

            case STATE_END:
                if( callback(ev, &param) == WBT_OK ) {
                    state = STATE_START;
                } else {
                    goto end;
                }
                break;
            default:
                return WBT_ERROR;
        }
    }
    
    end:
    if( state != STATE_KEY ) {
        return WBT_ERROR;
    } else {
        return WBT_OK;
    }
}

wbt_status wbt_bmtp2_on_send(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    wbt_bmtp2_t *bmtp = ev->data;

    while (!wbt_list_empty(&bmtp->send_list.head)) {
        wbt_bmtp2_msg_list_t *msg_node = wbt_list_first_entry(&bmtp->send_list.head, wbt_bmtp2_msg_list_t, head);
        
        while( msg_node->hed_offset < msg_node->hed_length ) {
            int ret = wbt_send(ev, msg_node->hed + msg_node->hed_offset , msg_node->hed_length - msg_node->hed_offset);
            if (ret <= 0) {
                if( wbt_socket_errno == WBT_EAGAIN ) {
                    return WBT_OK;
                } else {
                    wbt_log_add("wbt_send failed: %d\n", wbt_socket_errno );
                    return WBT_ERROR;
                }
            } else {
                msg_node->hed_offset += ret;
            }
        }
        
        if( msg_node->msg ) {
            while( msg_node->msg_offset < msg_node->msg->data_len ) {
                int ret = wbt_send(ev, msg_node->msg->data + msg_node->msg_offset , msg_node->msg->data_len - msg_node->msg_offset);
                if (ret <= 0) {
                    if( wbt_socket_errno == WBT_EAGAIN ) {
                        return WBT_OK;
                    } else {
                        wbt_log_add("wbt_send failed: %d\n", wbt_socket_errno );
                        return WBT_ERROR;
                    }
                } else {
                    msg_node->msg_offset += ret;
                }
            }
        }
        
        wbt_list_del(&msg_node->head);

        if( msg_node->msg ) {
            wbt_mq_msg_refer_dec(msg_node->msg);
        }
        
        wbt_free(msg_node->hed);
        wbt_free(msg_node);
    }
    
    if( !ev->is_exit ) {
        ev->events &= ~WBT_EV_WRITE;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_close(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->role == BMTP_CLIENT ) {
        wbt_mq_repl_on_close(ev);
    } else {
        wbt_mq_repl_client_delete(ev);
    }
    
    // 释放 send_list
    wbt_bmtp2_msg_list_t *msg_node;
    while(!wbt_list_empty(&bmtp->send_list.head)) {
        msg_node = wbt_list_first_entry(&bmtp->send_list.head, wbt_bmtp2_msg_list_t, head);

        wbt_list_del(&msg_node->head);

        if( msg_node->msg ) {
            wbt_mq_msg_refer_dec(msg_node->msg);
        }

        wbt_free( msg_node->hed );
        wbt_free( msg_node );
    }
    
    wbt_mq_auth_disconn(ev);

    wbt_log_add("BMTP close: %d\n", ev->fd);
    
    return WBT_OK;
}


wbt_status wbt_bmtp2_on_handler(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    wbt_bmtp2_t *bmtp = ev->data;

    if( bmtp->msg_offset > 0 ) {
        /* 删除已经处理完毕的消息
         */
        if( ev->buff_len == bmtp->msg_offset ) {
            wbt_free(ev->buff);
            ev->buff = NULL;
            ev->buff_len = 0;
            bmtp->recv_offset = 0;
        } else if( ev->buff_len > bmtp->msg_offset ) {
            wbt_memmove(ev->buff, (unsigned char *)ev->buff + bmtp->msg_offset, ev->buff_len - bmtp->msg_offset);
            ev->buff_len -= bmtp->msg_offset;
            bmtp->recv_offset -= bmtp->msg_offset;
        } else {
            wbt_log_add("BMTP error: unexpected error\n");
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_notify(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->window == 0 ) {
        return WBT_OK;
    }
    
    wbt_msg_t *msg;
    while( wbt_mq_pull(ev, &msg) == WBT_OK && msg) {
        if( wbt_bmtp2_send_pub(ev, msg) != WBT_OK ) {
            return WBT_ERROR;
        }
        
        if( bmtp->window > msg->data_len ) {
            bmtp->window -= msg->data_len;
        } else {
            bmtp->window = 0;
            break;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_send(wbt_event_t *ev, wbt_bmtp2_msg_list_t *node) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    wbt_log_debug( "bmtp: add to send queue\n" );
    wbt_list_add_tail(&node->head, &bmtp->send_list.head);

    ev->events |= WBT_EV_WRITE;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_append_opcode(wbt_bmtp2_msg_list_t *node, unsigned int op_code, unsigned char op_type, unsigned long long int l) {
    if( node->hed == NULL ) {
        node->hed_length = 20;
        node->hed_offset = 0;
        node->hed = wbt_malloc( node->hed_length );
        if( node->hed == NULL ) {
            return WBT_ERROR;
        }
    }
    
    int len = 0;
    int op = op_code;

    node->hed[len++] = ( ( ( op & 0xF ) | 0x10 ) << 3 ) + op_type;
    op >>= 4;

    while(op) {
        node->hed[len++] = ( op & 0x7F ) | 0x80;
        op >>= 7;
    };
    
    node->hed[len-1] &= 0x7F;
    
    if( op_code == OP_CONN ) {
        node->hed[len++] = 'B';
        node->hed[len++] = 'M';
        node->hed[len++] = 'T';
        node->hed[len++] = 'P';
    }
    
    int t;
    
    switch( op_type ) {
        case TYPE_BOOL:
            break;
        case TYPE_VARINT:
            do {
                node->hed[len++] = ( l & 0x7F ) | 0x80;
            } while( l >>= 7 );
            node->hed[len-1] &= 0x7F;
            break;
        case TYPE_64BIT:
            *((unsigned long long int *)(node->hed + len)) = l;
            len += 8;
            break;
        case TYPE_STRING:
            assert(node->hed_offset > 20);
            
            l = node->hed_offset - 20;
            if( node->msg ) {
                t = node->msg->data_len;
                do {
                    l --;
                } while(t >>= 7);
            }
            do {
                node->hed[len++] = ( l & 0x7F ) | 0x80;
            } while( l >>= 7 );
            node->hed[len-1] &= 0x7F;
            break;
        default:
            return WBT_ERROR;
    }
    
    if( node->hed_offset > 20 ) {
        node->hed_length = node->hed_offset;
    }
    
    node->hed_start  = 20 - len;
    node->hed_offset = 20 - len;
    wbt_memmove(node->hed + node->hed_offset, node->hed, len);
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_append_param(wbt_bmtp2_msg_list_t *node, unsigned char key, unsigned char key_type, unsigned long long int l, unsigned char *s) {
    if( node->hed == NULL ) {
        node->hed_length = 64;
        node->hed_offset = 20;
        node->hed = wbt_malloc( node->hed_length );
        if( node->hed == NULL ) {
            return WBT_ERROR;
        }
    }
    
    int space = 15;
    if( key_type == TYPE_STRING ) {
        space += l;
    }
    
    while( node->hed_length - node->hed_offset < space ) {
        node->hed_length *= 2;
    }
    
    unsigned char *p = wbt_realloc(node->hed, node->hed_length);
    if( p == NULL ) {
        return WBT_ERROR;
    }
    node->hed = p;
    
    int len = node->hed_offset;

    node->hed[len++] = ( ( ( key & 0xF ) | 0x10 ) << 3 ) + key_type;
    key >>= 4;

    while(key) {
        node->hed[len++] = ( key & 0x7F ) | 0x80;
        key >>= 7;
    };
    
    node->hed[len-1] &= 0x7F;
    
    int t;
    
    switch( key_type ) {
        case TYPE_BOOL:
            break;
        case TYPE_VARINT:
            do {
                node->hed[len++] = ( l & 0x7F ) | 0x80;
            } while( l >>= 7 );
            node->hed[len-1] &= 0x7F;
            break;
        case TYPE_64BIT:
            *((unsigned long long int *)(node->hed + len)) = l;
            len += 8;
            break;
        case TYPE_STRING:
            t = l;
            do {
                node->hed[len++] = ( t & 0x7F ) | 0x80;
            } while( t >>= 7 );
            node->hed[len-1] &= 0x7F;
            
            wbt_memcpy(node->hed + len, s, l);
            len += l;
            break;
        default:
            return WBT_ERROR;
    }
    
    node->hed_offset = len;

    return WBT_OK;
}

wbt_status wbt_bmtp2_append_payload(wbt_bmtp2_msg_list_t *node, wbt_msg_t *msg) {
    if( node->hed == NULL ) {
        node->hed_length = 64;
        node->hed_offset = 20;
        node->hed = wbt_malloc( node->hed_length );
        if( node->hed == NULL ) {
            return WBT_ERROR;
        }
    }
    
    int space = 1;
    
    while( node->hed_length - node->hed_offset < space ) {
        node->hed_length *= 2;
    }
    
    unsigned char *p = wbt_realloc(node->hed, node->hed_length);
    if( p == NULL ) {
        return WBT_ERROR;
    }
    node->hed = p;
    
    int len = node->hed_offset;
    
    int l = msg->data_len;
    do {
        node->hed[len++] = ( l & 0x7F ) | 0x80;
    } while( l >>= 7 );
    node->hed[len-1] &= 0x7F;
    
    node->hed_offset = len;

    /* 为了避免拷贝消息产生性能损耗，这里使用指针来直接读取消息内容。
     * 由此产生的问题是，消息可能会在发送的过程中过期。
     * 
     * 通过引用计数来避免消息过期
     */
    node->msg = msg;
    wbt_mq_msg_refer_inc(msg);
    
    return WBT_OK;
}