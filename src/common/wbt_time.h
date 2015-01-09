/* 
 * File:   wbt_time.h
 * Author: Fcten
 *
 * Created on 2015年1月9日, 上午10:51
 */

#ifndef __WBT_TIME_H__
#define	__WBT_TIME_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/time.h>

#include "wbt_memory.h"
#include "wbt_string.h"
    
extern time_t cur_mtime;
extern wbt_str_t wbt_time_str_log;
extern wbt_str_t wbt_time_str_http;

wbt_status wbt_time_init();
wbt_status wbt_time_update();

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_TIME_H__ */

