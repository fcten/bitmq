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
extern wbt_module_t wbt_module_helloworld;

wbt_module_t * wbt_modules[] = {
    &wbt_module_time,
    &wbt_module_log,
    &wbt_module_conf,
    &wbt_module_event,
    &wbt_module_conn,
    &wbt_module_file,
    &wbt_module_http,
    &wbt_module_helloworld,
    NULL
};

wbt_status wbt_module_init() {
    /* 初始化模块 */
    int i , size = sizeof(wbt_modules)/sizeof(wbt_module_t *) - 1;
    for( i = 0 ; i < size ; i++ ) {
        wbt_log_print( "\nInitialize module %-10.*s [        ]", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        if( wbt_modules[i]->init && wbt_modules[i]->init(/*cycle*/) != WBT_OK ) {
            /* fatal */
            wbt_log_print( "\rInitialize module %-10.*s [ \033[31;49;1mFAILED\033[0m ]", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
            return WBT_ERROR;
        } else {
            wbt_log_print( "\rInitialize module %-10.*s [   \033[32;49;1mOK\033[0m   ]", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        }
    }
    return WBT_OK;
}

wbt_status wbt_module_exit() {
    /* 卸载所有模块 */
    int i, size = sizeof(wbt_modules)/sizeof(wbt_module_t *) - 1;
    for( i = size - 1 ; i >= 0 ; i-- ) {
        wbt_log_print( "\nFinalize module %-12.*s [        ]", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        if( wbt_modules[i]->exit && wbt_modules[i]->exit(/*cycle*/) != WBT_OK ) {
            /* fatal */
            wbt_log_print( "\rFinalize module %-12.*s [ \033[31;49;1mFAILED\033[0m ]", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        } else {
            wbt_log_print( "\rFinalize module %-12.*s [   \033[32;49;1mOK\033[0m   ]", wbt_modules[i]->name.len, wbt_modules[i]->name.str );
        }
    }
    return WBT_OK;
}

wbt_status wbt_module_on_filter(wbt_http_t *http) {
    /* filter模块 */
    int i , size = sizeof(wbt_modules)/sizeof(wbt_module_t *) - 1;
    for( i = 0 ; i < size ; i++ ) {
        if( wbt_modules[i]->on_filter && wbt_modules[i]->on_filter(http) != WBT_OK ) {
            /* fatal */
            return WBT_ERROR;
        }
    }
    return WBT_OK;
}