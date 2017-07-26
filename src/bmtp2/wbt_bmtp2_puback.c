#include "wbt_bmtp2.h"

enum {
    PARAM_STREAM_ID = 1,
    PARAM_MSG_ID,
    PARAM_STATUS
};

wbt_status wbt_bmtp2_on_puback(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->role != BMTP_CLIENT ) {
        // 说明：BitMQ 不依赖 BMTP 协议提供消息投递的质量保证。
        // BitMQ 向客户端投递的消息均不包含 stream_id。客户端亦
        // 无需向 BitMQ 发送 PUBACK 响应。
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_bmtp2_send_puback(wbt_event_t *ev, unsigned long long int stream_id, unsigned char status) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }
    
    //wbt_log_add("stream_id: %lld\n" ,stream_id );

    wbt_bmtp2_append_param(node, PARAM_STREAM_ID, TYPE_VARINT, stream_id, NULL);
    wbt_bmtp2_append_param(node, PARAM_STATUS, TYPE_VARINT, status, NULL);

    wbt_bmtp2_append_opcode(node, OP_PUBACK, TYPE_STRING, 0);
    
    return wbt_bmtp2_send(ev, node);
}