/* 
 * File:   wbt_module.c
 * Author: Fcten
 *
 * Created on 2014年10月23日, 下午2:46
 */

#include "wbt_module.h"
#include "wbt_log.h"

extern wbt_module_t wbt_module_time;
extern wbt_module_t wbt_module_log;
extern wbt_module_t wbt_module_conf;
extern wbt_module_t wbt_module_event;
extern wbt_module_t wbt_module_conn;
extern wbt_module_t wbt_module_file;
extern wbt_module_t wbt_module_http;

wbt_module_t * wbt_modules[] = {
    &wbt_module_time,
    &wbt_module_log,
    &wbt_module_conf,
    &wbt_module_event,
    &wbt_module_conn,
    &wbt_module_file,
    &wbt_module_http,
    NULL
};

wbt_status wbt_module_init() {
    /* 初始化模块 */
    int i , size = sizeof(wbt_modules)/sizeof(wbt_module_t *) - 1;
    for( i = 0 ; i < size ; i++ ) {
        if( wbt_modules[i]->init && wbt_modules[i]->init(/*cycle*/) != WBT_OK ) {
            /* fatal */
            wbt_log_debug( "module %.*s occured errors", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
            return WBT_ERROR;
        } else {
            wbt_log_debug( "module %.*s loaded", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        }
    }
    return WBT_OK;
}
wbt_status wbt_module_exit() {
    /* 卸载所有模块 */
    int i, size = sizeof(wbt_modules)/sizeof(wbt_module_t *) - 1;
    for( i = size - 1 ; i >= 0 ; i-- ) {
        if( wbt_modules[i]->exit && wbt_modules[i]->exit(/*cycle*/) != WBT_OK ) {
            /* fatal */
            wbt_log_debug( "module %.*s occured errors", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        } else {
            wbt_log_debug( "module %.*s exit", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        }
    }
    return WBT_OK;
}
