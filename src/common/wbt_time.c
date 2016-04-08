/* 
 * File:   wbt_time.c
 * Author: Fcten
 *
 * Created on 2015年1月9日, 上午10:51
 */

#include "../webit.h"
#include "wbt_time.h"
#include "wbt_memory.h"
#include "wbt_log.h"
#include "wbt_module.h"

wbt_module_t wbt_module_time = {
    wbt_string("time"),
    wbt_time_init
};

/* 当前时间戳，单位毫秒 */
time_t wbt_cur_mtime;

wbt_str_t wbt_time_str_log;
wbt_str_t wbt_time_str_http;
wbt_str_t wbt_time_str_expire;
wbt_str_t wbt_time_str_tmp;

wbt_status wbt_time_init() {
    wbt_time_str_log.len = sizeof("[05/06/91 00:00:00] ") - 1;
    wbt_time_str_log.str = wbt_malloc( wbt_time_str_log.len + 1 );

    wbt_time_str_http.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
    wbt_time_str_http.str = wbt_malloc( wbt_time_str_http.len + 1 );
    
    wbt_time_str_expire.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
    wbt_time_str_expire.str = wbt_malloc( wbt_time_str_expire.len + 1 );

    wbt_time_str_tmp.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
    wbt_time_str_tmp.str = wbt_malloc( wbt_time_str_tmp.len + 1 );

    wbt_time_update();
    wbt_time_str_update();

    return WBT_OK;
}

wbt_status wbt_time_update() {
    struct timeval cur_utime;
	gettimeofday(&cur_utime, NULL);
    wbt_cur_mtime = 1000 * cur_utime.tv_sec + cur_utime.tv_usec / 1000;

    return WBT_OK;
}

wbt_status wbt_time_str_update() {
    time_t now;
    struct tm *timenow;

    now = time(NULL);
    timenow = localtime(&now);
    strftime(wbt_time_str_log.str, wbt_time_str_log.len + 1, "[%x %X] ", timenow);
    timenow = gmtime(&now);
    strftime(wbt_time_str_http.str, wbt_time_str_http.len + 1, "%a, %d %b %Y %H:%M:%S GMT", timenow);
    now += 3600;
    timenow = gmtime(&now);
    strftime(wbt_time_str_expire.str, wbt_time_str_expire.len + 1, "%a, %d %b %Y %H:%M:%S GMT", timenow);
    
    return WBT_OK;
}

wbt_str_t * wbt_time_to_str( time_t time ) {
    struct tm *timenow;
    timenow = gmtime(&time);
    strftime(wbt_time_str_tmp.str, wbt_time_str_tmp.len + 1, "%a, %d %b %Y %H:%M:%S GMT", timenow);
    
    return &wbt_time_str_tmp;
}