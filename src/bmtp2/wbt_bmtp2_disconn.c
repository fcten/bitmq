#include "wbt_bmtp2.h"

wbt_status wbt_bmtp2_on_disconn(wbt_event_t *ev) {
    ev->is_exit = 1;
    
    wbt_log_add("BMTP recvived disconn\n");
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_disconn(wbt_event_t *ev) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_opcode(node, OP_DISCONN, TYPE_BOOL, 0);
    
    return wbt_bmtp2_send(ev, node);
}