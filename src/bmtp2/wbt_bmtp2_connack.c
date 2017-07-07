#include "wbt_bmtp2.h"

wbt_status wbt_bmtp2_on_connack(wbt_event_t *ev) {
    // BitMQ 不接受任何 CONNACK 消息
    return WBT_ERROR;
}

wbt_status wbt_bmtp2_send_connack(wbt_event_t *ev, unsigned char status) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_opcode(node, OP_CONNACK, TYPE_VARINT, status);

    return wbt_bmtp2_send(ev, node);
}