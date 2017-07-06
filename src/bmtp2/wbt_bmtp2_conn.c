#include "wbt_bmtp2.h"

enum {
    PARAM_AUTH = 1
};

enum {
    RET_OK = 0,
    RET_SERVICE_UNAVAILABLE,
    RET_INVALID_REQUEST,
    RET_INVALID_TOKEN,
    RET_TOO_MANY_CONNECTIONS
};

wbt_status wbt_bmtp2_on_connect_parser(wbt_event_t *ev, wbt_bmtp2_param_t *param) {
    switch( param->key ) {
        case PARAM_AUTH:
            if( param->key_type != TYPE_STRING ) {
                wbt_bmtp2_send_connack(ev, RET_INVALID_REQUEST);
                return WBT_ERROR;
            }
            
            if( wbt_conf.auth == 1 ) {
                // TODO basic 验证
                
                wbt_mq_set_auth(ev, NULL);
            } else if ( wbt_conf.auth == 2 ) {
                // standard 验证
                wbt_str_t token, sign, split = wbt_string(".");
                token.str = (char *)param->value.s;
                token.len = param->value.l;

                int pos = wbt_strpos(&token, &split);
                if(pos == -1) {
                    wbt_bmtp2_send_connack(ev, RET_INVALID_TOKEN);
                    return WBT_ERROR;
                }
                sign.str = token.str+pos+1;
                sign.len = token.len-pos-1;

                token.len = pos;

                if(wbt_auth_verify(&token, &sign) != WBT_OK) {
                    wbt_bmtp2_send_connack(ev, RET_INVALID_TOKEN);
                    return WBT_ERROR;
                }

                wbt_str_t token_decode;
                token_decode.len = token.len;
                token_decode.str = (char *) wbt_malloc(token.len);
                if( token_decode.str == NULL ) {
                    wbt_bmtp2_send_connack(ev, RET_SERVICE_UNAVAILABLE);
                    return WBT_ERROR;
                }

                if( wbt_base64_decode(&token_decode, &token) != WBT_OK ) {
                    wbt_free(token_decode.str);
                    
                    wbt_bmtp2_send_connack(ev, RET_INVALID_TOKEN);
                    return WBT_ERROR;
                }

                // 读取授权信息
                wbt_auth_t *auth = wbt_mq_auth_create(&token_decode);
                if( auth == NULL ) {
                    wbt_free(token_decode.str);
                    
                    wbt_bmtp2_send_connack(ev, RET_INVALID_TOKEN);
                    return WBT_ERROR;
                }

                wbt_free(token_decode.str);

                wbt_mq_set_auth(ev, auth);
            }

            return WBT_OK;
        default:
            // 忽略无法识别的参数
            return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_connect(wbt_event_t *ev) {
    wbt_log_debug("new conn\n");

    wbt_bmtp2_t *bmtp = ev->data;

    if( wbt_mq_login(ev) != WBT_OK ) {
        ev->is_exit = 1;
        return wbt_bmtp2_send_connack(ev, RET_SERVICE_UNAVAILABLE);
    }
    
#ifdef WITH_WEBSOCKET
    wbt_mq_set_notify(ev, wbt_websocket_notify);
#else
    wbt_mq_set_notify(ev, wbt_bmtp2_notify);
#endif

    // 默认设定为 anonymous 授权
    wbt_mq_set_auth(ev, wbt_mq_auth_anonymous());
    
    if( wbt_bmtp2_param_parser(ev, wbt_bmtp2_on_connect_parser) != WBT_OK ) {
        ev->is_exit = 1;
        return WBT_OK;
    }

    if( wbt_mq_auth_conn_limit(ev) != WBT_OK ) {
        ev->is_exit = 1;
        return wbt_bmtp2_send_connack(ev, RET_TOO_MANY_CONNECTIONS);
    }

    bmtp->is_conn = 1;
    
    return wbt_bmtp2_send_connack(ev, RET_OK);
}

wbt_status wbt_bmtp2_send_conn(wbt_event_t *ev, wbt_str_t *auth) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    if( auth ) {
        wbt_bmtp2_append_param(node, PARAM_AUTH, TYPE_STRING, (unsigned int)auth->len, (unsigned char*)auth->str);
        wbt_bmtp2_append_opcode(node, OP_CONN, TYPE_STRING, 0);
    } else {
        wbt_bmtp2_append_opcode(node, OP_CONN, TYPE_BOOL, 0);
    }
    
    return wbt_bmtp2_send(ev, node);
}