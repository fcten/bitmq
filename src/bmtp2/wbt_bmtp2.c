/* 
 * File:   wbt_bmtp2.c
 * Author: fcten
 *
 * Created on 2017年6月19日, 下午9:31
 */

#include "../webit.h"
#include "../event/wbt_event.h"
#include "../common/wbt_module.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_config.h"
#include "../common/wbt_log.h"
#include "../common/wbt_auth.h"
#include "../common/wbt_base64.h"
#include "../json/wbt_json.h"
#include "../mq/wbt_mq.h"
#include "../mq/wbt_mq_msg.h"
#include "../mq/wbt_mq_auth.h"
#ifdef WITH_WEBSOCKET
#include "../websocket/wbt_websocket.h"
#endif
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

wbt_status wbt_bmtp2_param_begin(wbt_event_t *ev);
wbt_status wbt_bmtp2_param_end(wbt_event_t *ev);

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
                
                wbt_status ret = WBT_ERROR;
                if( wbt_bmtp2_param_begin(ev) == WBT_OK ) {
                    ret = wbt_bmtp2_cmd_table[bmtp->op_code].on_proc(ev);
                }
                wbt_bmtp2_param_end(ev);
                
                if( ret == WBT_OK ) {
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

wbt_status wbt_bmtp2_param_begin(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->op_type != TYPE_STRING ) {
        return WBT_OK;
    }
    
    wbt_list_init(&bmtp->param.head);
    
    int state = STATE_START;
    int offset = 0;
    wbt_bmtp2_param_list_t param, *new_param;
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
                new_param = wbt_strdup(&param, sizeof(param));
                if( new_param == NULL ) {
                    return WBT_ERROR;
                }
                
                wbt_list_add_tail(&new_param->head, &bmtp->param.head);
                
                state = STATE_START;
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

wbt_status wbt_bmtp2_param_end(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->op_type != TYPE_STRING ) {
        return WBT_OK;
    }
    
    wbt_bmtp2_param_list_t *param;
    while(!wbt_list_empty(&bmtp->param.head)) {
        param = wbt_list_first_entry(&bmtp->param.head, wbt_bmtp2_param_list_t, head);
        wbt_list_del(&param->head);
        wbt_free( param );
    }
    
    return WBT_OK;
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

enum {
    CONN_AUTH = 0
};

wbt_status wbt_bmtp2_on_connect(wbt_event_t *ev) {
    wbt_log_debug("new conn\n");

    wbt_bmtp2_t *bmtp = ev->data;
    
    wbt_bmtp2_param_list_t *param_auth = NULL;
    
    // 授权验证
    wbt_auth_t *auth = wbt_mq_auth_anonymous();
    if( wbt_conf.auth == 1 ) {
        // basic 验证
        if( param_auth != NULL ) {
        
            // 通过 basic 验证的客户端拥有不受限制的访问授权
            auth = NULL;
        } else {
            if( auth == NULL ) {
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - anonymous not allowed\n");
                return wbt_bmtp2_send_connack(ev, 0x3);
            }
        }
    } else if( wbt_conf.auth == 2 ) {
        // standard 验证
        if( param_auth != NULL ) {
            wbt_str_t token, sign, split = wbt_string(".");
            token.str = (char *)param_auth->value.s;
            token.len = param_auth->value.l;

            int pos = wbt_strpos(&token, &split);
            if(pos == -1) {
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - invalid token\n");
                return wbt_bmtp2_send_connack(ev, 0x3);
            }
            sign.str = token.str+pos+1;
            sign.len = token.len-pos-1;

            token.len = pos;

            if(wbt_auth_verify(&token, &sign) != WBT_OK) {
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - verify failed\n");
                return wbt_bmtp2_send_connack(ev, 0x3);
            }

            wbt_str_t token_decode;;
            token_decode.len = token.len;
            token_decode.str = (char *) wbt_malloc(token.len);
            if( token_decode.str == NULL ) {
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - out of memory\n");
                return wbt_bmtp2_send_connack(ev, 0x2);
            }

            if( wbt_base64_decode(&token_decode, &token) != WBT_OK ) {
                wbt_free(token_decode.str);
                
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - base64 decode failed\n");
                return wbt_bmtp2_send_connack(ev, 0x3);
            }
            
            // 读取授权信息
            auth = wbt_mq_auth_create(&token_decode);
            if( auth == NULL ) {
                wbt_free(token_decode.str);
                
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - auth create failed\n");
                return wbt_bmtp2_send_connack(ev, 0x3);
            }
            
            wbt_free(token_decode.str);
        } else {
            if( auth == NULL ) {
                ev->is_exit = 1;
                wbt_log_add("BMTP error: authorize failed - anonymous not allowed\n");
                return wbt_bmtp2_send_connack(ev, 0x3);
            }
        }
    }

    if( wbt_mq_login(ev) != WBT_OK ) {
        return wbt_bmtp2_send_connack(ev, 0x2);
    }
    
#ifdef WITH_WEBSOCKET
    wbt_mq_set_notify(ev, wbt_websocket_notify);
#else
    wbt_mq_set_notify(ev, wbt_bmtp_notify);
#endif
    wbt_mq_set_auth(ev, auth);

    if( wbt_mq_auth_conn_limit(ev) != WBT_OK ) {
        ev->is_exit = 1;
        wbt_log_add("BMTP error: too many connections\n");
        return wbt_bmtp2_send_connack(ev, 0x4);
    }

    bmtp->is_conn = 1;
    
    return wbt_bmtp2_send_connack(ev, 0x0);
}

wbt_status wbt_bmtp2_on_connack(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_pub(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_puback(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_sub(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_suback(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_ping(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_pingack(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_disconn(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_window(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_notify(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_conn(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_connack(wbt_event_t *ev, unsigned char status) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_pub(wbt_event_t *ev, wbt_msg_t *msg) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_puback(wbt_event_t *ev, unsigned char status) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_sub(wbt_event_t *ev, unsigned long long int channel_id) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_suback(wbt_event_t *ev, unsigned char status) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_ping(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_pingack(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_disconn(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_window(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp2_send(wbt_event_t *ev, char *buf, int len) {
    return WBT_OK;
}
