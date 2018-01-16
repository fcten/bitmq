#include "wbt_bmtp2.h"

enum {
    PARAM_CHANNEL_ID = 1
};

enum {
    RET_OK = 0,
    RET_SERVICE_UNAVAILABLE,
    RET_PERMISSION_DENIED
};

wbt_status wbt_bmtp2_on_sub_parser(wbt_event_t *ev, wbt_bmtp2_param_t *param) {
    wbt_mq_id channel_id;
    
    switch( param->key ) {
        case PARAM_CHANNEL_ID:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    channel_id = param->value.l;
                    break;
                case TYPE_STRING:
                    // TODO 暂不支持字符串
                    return WBT_OK;
                default:
                    return WBT_ERROR;
            }

            if( wbt_mq_auth_sub_permission(ev, channel_id) != WBT_OK ) {
                // permission denied
                return wbt_bmtp2_send_suback(ev, channel_id, RET_PERMISSION_DENIED);
            }

            // 在所有想要订阅的频道的 subscriber_list 中添加该订阅者
            if( wbt_mq_subscribe(ev, channel_id) != WBT_OK ) {
                return wbt_bmtp2_send_suback(ev, channel_id, RET_SERVICE_UNAVAILABLE);
            }

            return wbt_bmtp2_send_suback(ev, channel_id, RET_OK);
        default:
            // 忽略无法识别的参数
            return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_sub(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->role != BMTP_SERVER ) {
        return WBT_ERROR;
    }
    
    switch( bmtp->op_type ) {
        case TYPE_STRING:
            if( wbt_bmtp2_param_parser(ev, wbt_bmtp2_on_sub_parser) != WBT_OK ) {
                ev->is_exit = 1;
                return WBT_OK;
            }
            break;
        case TYPE_64BIT:
        case TYPE_VARINT:
            if( wbt_mq_auth_sub_permission(ev, bmtp->op_value.l) != WBT_OK ) {
                // permission denied
                wbt_bmtp2_send_suback(ev, bmtp->op_value.l, RET_PERMISSION_DENIED);
                break;
            }

            // 在所有想要订阅的频道的 subscriber_list 中添加该订阅者
            if( wbt_mq_subscribe(ev, bmtp->op_value.l) != WBT_OK ) {
                wbt_bmtp2_send_suback(ev, bmtp->op_value.l, RET_SERVICE_UNAVAILABLE);
                break;
            }

            wbt_bmtp2_send_suback(ev, bmtp->op_value.l, RET_OK);
            break;
        default:
            return WBT_ERROR;
    }

    return wbt_bmtp2_notify(ev);
}

wbt_status wbt_bmtp2_send_sub(wbt_event_t *ev, wbt_mq_id channel_id) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_param(node, PARAM_CHANNEL_ID, TYPE_VARINT, channel_id, NULL);

    wbt_bmtp2_append_opcode(node, OP_SUB, TYPE_STRING, 0);
    
    return wbt_bmtp2_send(ev, node);
}