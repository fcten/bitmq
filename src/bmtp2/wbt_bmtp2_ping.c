#include "wbt_bmtp2.h"

wbt_status wbt_bmtp2_on_ping(wbt_event_t *ev) {
    wbt_log_debug("new ping\n");
    
    return wbt_bmtp2_send_pingack(ev);
}

wbt_status wbt_bmtp2_send_ping(wbt_event_t *ev) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_opcode(node, OP_PING, TYPE_BOOL, 0);
    
    return wbt_bmtp2_send(ev, node);
}