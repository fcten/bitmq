#include "wbt_bmtp2.h"

enum {
    PARAM_STREAM_ID = 1,
    PARAM_MSG_ID,
    PARAM_TYPE,
    PARAM_PRODUCER_ID,
    PARAM_CONSUMER_ID,
    PARAM_CREATE,
    PARAM_EFFECT,
    PARAM_EXPIRE
};

enum {
    RET_OK = 0,
    RET_SERVICE_UNAVAILABLE,
    RET_PERMISSION_DENIED,
    RET_INVALID_MESSAGE
};

// TODO 使用全局变量不是多线程安全的，如果未来 BitMQ 引入了多线程，这里可能带来问题
wbt_msg_t wbt_mq_parsed_msg;
wbt_mq_id stream_id;

wbt_status wbt_bmtp2_on_pub_parser(wbt_event_t *ev, wbt_bmtp2_param_t *param) {
    switch( param->key ) {
        case PARAM_STREAM_ID:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    stream_id = param->value.l;
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_MSG_ID:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    wbt_mq_parsed_msg.msg_id = param->value.l;
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_TYPE:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    switch(param->value.l) {
                        case MSG_BROADCAST:
                            wbt_mq_parsed_msg.qos  = 0;
                            wbt_mq_parsed_msg.type = MSG_BROADCAST;
                            break;
                        case MSG_LOAD_BALANCE:
                            wbt_mq_parsed_msg.qos  = 1;
                            wbt_mq_parsed_msg.type = MSG_LOAD_BALANCE;
                            break;
                        case MSG_ACK:
                            wbt_mq_parsed_msg.qos  = 0;
                            wbt_mq_parsed_msg.type = MSG_ACK;
                            break;
                        default:
                            return WBT_ERROR;
                    }
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_PRODUCER_ID:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    wbt_mq_parsed_msg.producer_id = param->value.l;
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_CONSUMER_ID:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    wbt_mq_parsed_msg.consumer_id = param->value.l;
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_CREATE:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    wbt_mq_parsed_msg.create = param->value.l;
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_EFFECT:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    if( param->value.l >= 0 && param->value.l <= 2592000 ) {
                        wbt_mq_parsed_msg.effect = param->value.l;
                    } else {
                        return WBT_ERROR;
                    }
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        case PARAM_EXPIRE:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    if( param->value.l >= 0 && param->value.l <= 2592000 ) {
                        wbt_mq_parsed_msg.expire = param->value.l;
                    } else {
                        return WBT_ERROR;
                    }
                    break;
                default:
                    return WBT_ERROR;
            }
            break;
        default:
            // 忽略无法识别的参数
            return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_pub(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    stream_id = 0;
    wbt_memset(&wbt_mq_parsed_msg, 0, sizeof(wbt_mq_parsed_msg));
    
    if( wbt_bmtp2_param_parser(ev, wbt_bmtp2_on_pub_parser) != WBT_OK ) {
        if( stream_id ) {
            return wbt_bmtp2_send_puback(ev, stream_id, RET_INVALID_MESSAGE);
        } else {
            return WBT_OK;
        }
    }
    
    if( !wbt_mq_parsed_msg.consumer_id || !bmtp->payload_length ) {
        if( stream_id ) {
            return wbt_bmtp2_send_puback(ev, stream_id, RET_INVALID_MESSAGE);
        } else {
            return WBT_OK;
        }
    }
    
    wbt_mq_parsed_msg.data_len = bmtp->payload_length;
    wbt_mq_parsed_msg.data = wbt_strdup(bmtp->payload, bmtp->payload_length);
    
    if( stream_id == 0 ) {
        wbt_mq_push(ev, &wbt_mq_parsed_msg);
        return WBT_OK;
    } else {
        if( wbt_mq_push(ev, &wbt_mq_parsed_msg) != WBT_OK ) {
            // TODO 需要返回更详细的错误原因
            return wbt_bmtp2_send_puback(ev, stream_id, RET_PERMISSION_DENIED);
        } else {
            return wbt_bmtp2_send_puback(ev, stream_id, RET_OK);
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_pub(wbt_event_t *ev, wbt_msg_t *msg) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_bmtp2_append_opcode(node, OP_PUB, TYPE_BOOL, 0);
    
    /* 为了避免拷贝消息产生性能损耗，这里使用指针来直接读取消息内容。
     * 由此产生的问题是，消息可能会在发送的过程中过期。
     * 
     * 通过引用计数来避免消息过期
     */
    node->msg = msg;
    wbt_mq_msg_refer_inc(msg);
    
    return wbt_bmtp2_send(ev, node);
}