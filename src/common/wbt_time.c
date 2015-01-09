/* 
 * File:   wbt_time.c
 * Author: Fcten
 *
 * Created on 2015年1月9日, 上午10:51
 */

#include "../webit.h"
#include "wbt_time.h"

/* 当前时间戳，单位毫秒 */
time_t cur_mtime;

wbt_status wbt_time_update() {
    struct timeval cur_utime;
    gettimeofday(&cur_utime, NULL);
    cur_mtime = 1000 * cur_utime.tv_sec + cur_utime.tv_usec / 1000;
    
    return WBT_OK;
}