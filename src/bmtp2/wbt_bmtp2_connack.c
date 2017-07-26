#include "wbt_bmtp2.h"
#include "../mq/wbt_mq_replication.h"

wbt_status wbt_bmtp2_on_connack(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->role != BMTP_CLIENT ) {
        return WBT_ERROR;
    }
    
    bmtp->is_conn = 1;
    
    wbt_mq_repl_on_open(ev);

    return WBT_OK;
}

wbt_status wbt_bmtp2_send_connack(wbt_event_t *ev, unsigned char status) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_opcode(node, OP_CONNACK, TYPE_VARINT, status);

    return wbt_bmtp2_send(ev, node);
}