/* 
 * File:   wbt_module.c
 * Author: Fcten
 *
 * Created on 2014年10月23日, 下午2:46
 */

#include "wbt_module.h"

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