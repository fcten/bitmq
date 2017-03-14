 /* 
 * File:   wbt_websocket.c
 * Author: Fcten
 *
 * Created on 2017年01月17日, 下午4:08
 */

#include "../common/wbt_module.h"
#include "../common/wbt_memory.h"
#include "../common/wbt_log.h"
#include "../common/wbt_time.h"
#include "../common/wbt_config.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_ssl.h"
#include "../common/wbt_string.h"
#include "../common/wbt_file.h"
#include "../common/wbt_gzip.h"
#include "wbt_websocket.h"

extern wbt_atomic_t wbt_wating_to_exit;

wbt_module_t wbt_module_websocket = {
    wbt_string("websocket"),
    wbt_websocket_init,
    wbt_websocket_exit,
    NULL, // websocket 连接需要由 http 连接升级而来
    wbt_websocket_on_recv,
    wbt_websocket_on_send,
    wbt_websocket_on_close
};

wbt_status wbt_websocket_init() {
    return WBT_OK;
}

wbt_status wbt_websocket_exit() {
    return WBT_OK;
}

wbt_status wbt_websocket_on_conn( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_websocket_on_send( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }
    
    return WBT_OK;
}

wbt_status wbt_websocket_on_close( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }

    return WBT_OK;
}

wbt_status wbt_websocket_on_recv( wbt_event_t *ev ) {
    if( ev->protocol != WBT_PROTOCOL_WEBSOCKET ) {
        return WBT_OK;
    }
    
    return WBT_OK;
}