#include "wbt_bmtp2.h"

enum {
    PARAM_CHANNEL_ID = 1,
    PARAM_STATUS
};

wbt_status wbt_bmtp2_on_suback(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->role != BMTP_CLIENT ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_suback(wbt_event_t *ev, wbt_mq_id channel_id, unsigned char status) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_param(node, PARAM_CHANNEL_ID, TYPE_VARINT, channel_id, NULL);
    wbt_bmtp2_append_param(node, PARAM_STATUS, TYPE_VARINT, status, NULL);

    wbt_bmtp2_append_opcode(node, OP_SUBACK, TYPE_STRING, 0);
    
    return wbt_bmtp2_send(ev, node);
}