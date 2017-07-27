#include "wbt_bmtp2.h"
#include "mq/wbt_mq_replication.h"

enum {
    PARAM_MSG_ID = 1
};

wbt_status wbt_bmtp2_on_sync(wbt_event_t *ev) {
    wbt_bmtp2_t *bmtp = ev->data;
    
    if( bmtp->role != BMTP_SERVER ) {
        return WBT_ERROR;
    }
    
    wbt_repl_cli_t *cli = NULL;

    switch( bmtp->op_type ) {
        case TYPE_64BIT:
        case TYPE_VARINT:
            cli = wbt_mq_repl_client_get(ev);
            if( cli == NULL ) {
                cli = wbt_mq_repl_client_new(ev);
                if( cli == NULL ) {
                    return WBT_ERROR;
                }
            }
            cli->msg_id = bmtp->op_value.l;
            
            //wbt_mq_repl_send(cli);
            break;
        default:
            return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_send_sync(wbt_event_t *ev, wbt_mq_id msg_id) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    //wbt_bmtp2_append_param(node, PARAM_MSG_ID, TYPE_VARINT, msg_id, NULL);

    wbt_bmtp2_append_opcode(node, OP_SYNC, TYPE_VARINT, msg_id);
    
    return wbt_bmtp2_send(ev, node);
}