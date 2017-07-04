#include "wbt_bmtp2.h"

enum {
    PARAM_WINDOW_SIZE = 0
};

wbt_status wbt_bmtp2_on_window_parser(wbt_event_t *ev, wbt_bmtp2_param_t *param) {
    wbt_bmtp2_t *bmtp = ev->data;
    unsigned int window_size;
    
    switch( param->key ) {
        case PARAM_WINDOW_SIZE:
            switch(param->key_type) {
                case TYPE_VARINT:
                case TYPE_64BIT:
                    window_size = param->value.l;
                    break;
                default:
                    return WBT_ERROR;
            }

            bmtp->window = window_size;
            break;
        default:
            // 忽略无法识别的参数
            return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp2_on_window(wbt_event_t *ev) {
    if( wbt_bmtp2_param_parser(ev, wbt_bmtp2_on_window_parser) != WBT_OK ) {
        ev->is_exit = 1;
        return WBT_OK;
    }

    return wbt_bmtp2_notify(ev);
}

wbt_status wbt_bmtp2_send_window(wbt_event_t *ev, unsigned int size) {
    wbt_bmtp2_msg_list_t *node = wbt_calloc(sizeof(wbt_bmtp2_msg_list_t));
    if( node == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp2_append_param(node, PARAM_WINDOW_SIZE, TYPE_VARINT, size, NULL);
    
    wbt_bmtp2_append_opcode(node, OP_WINDOW, TYPE_STRING, 0);
    
    return wbt_bmtp2_send(ev, node);
}