/* 
 * File:   wbt_bmtp2.c
 * Author: fcten
 *
 * Created on 2017年6月19日, 下午9:31
 */

#include "wbt_bmtp2.h"

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
    {11, wbt_string("RSV"),     NULL                 }
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
    
    bmtp->state = STATE_START;

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
                
                bmtp->state = STATE_END;
                break;
            case STATE_VALUE_64BIT:
                if( bmtp->recv_offset + 8 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                bmtp->op_value.l = (unsigned long long int)( ev->buff + bmtp->recv_offset );
                
                bmtp->recv_offset += 8;

                bmtp->state = STATE_END;
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
                    bmtp->state = STATE_END;
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
                
                bmtp->op_value.l += ( c & 0x7F ) << ( ( bmtp->state - STATE_VALUE_VARINT ) * 7 );
                
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
                        bmtp->state = STATE_END;
                    }
                }
                break;
            case STATE_VALUE_STRING:
                if( bmtp->recv_offset + bmtp->op_value.l > ev->buff_len ) {
                    if( bmtp->recv_offset - bmtp->msg_offset + bmtp->op_value.l > WBT_MAX_PROTO_BUF_LEN ) {
                        wbt_log_add("BMTP error: message length exceeds limitation\n");
                        return WBT_ERROR;
                    } else {
                        return WBT_OK;
                    }
                }
                
                bmtp->op_value.s = (unsigned char *)(ev->buff + bmtp->recv_offset );
                
                bmtp->recv_offset += bmtp->op_value.l;

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
                
                if( !bmtp->is_conn && bmtp->op_code != OP_CONN ) {
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
                
                param.value.l += ( c & 0x7F ) << ( ( state - STATE_VALUE_VARINT ) * 7 );
                
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
                
                param.value.l = (unsigned long long int)( buf + offset );
                
                offset += 8;

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
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_close(wbt_event_t *ev) {
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
    return WBT_OK;
}

wbt_status wbt_bmtp2_send(wbt_event_t *ev, char *buf, int len) {
    return WBT_OK;
}
