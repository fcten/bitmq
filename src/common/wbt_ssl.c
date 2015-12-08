 /* 
 * File:   wbt_http.c
 * Author: Fcten
 *
 * Created on 2014年10月24日, 下午3:30
 */

#include "wbt_module.h"
#include "wbt_ssl.h"

wbt_module_t wbt_module_ssl = {
    wbt_string("ssl"),
    wbt_ssl_init,
    wbt_ssl_exit,
    wbt_ssl_on_conn,
    wbt_ssl_on_recv,
    wbt_ssl_on_send,
    wbt_ssl_on_close
};

wbt_status wbt_ssl_init() {
    return WBT_OK;
}

wbt_status wbt_ssl_exit() {
    return WBT_OK;
}

wbt_status wbt_ssl_on_conn( wbt_event_t * ev ) {
    return WBT_OK;
}

wbt_status wbt_ssl_on_recv( wbt_event_t * ev ) {
    return WBT_OK;
}

wbt_status wbt_ssl_on_send( wbt_event_t * ev ) {
    return WBT_OK;
}

wbt_status wbt_ssl_on_close( wbt_event_t * ev ) {
    return WBT_OK;
}
