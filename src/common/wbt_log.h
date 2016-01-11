/* 
 * File:   wbt_log.h
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午4:05
 */

#ifndef __WBT_LOG_H__
#define	__WBT_LOG_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "../webit.h"
#include "wbt_time.h"
#include "wbt_string.h"

typedef enum {
    WBT_LOG_OK,
    WBT_LOG_WARNNING,
    WBT_LOG_ERROR,
    WBT_LOG_DEBUG
} wbt_log_level_t;

wbt_status wbt_log_init();
wbt_status wbt_log_add(const char *fmt, ...);
wbt_status wbt_log_print(const char *fmt, ...);

#ifdef WBT_DEBUG
#define wbt_log_debug(fmt, arg...) \
	printf ("\n\033[31;49;1mDEBUG\033[0m ~ %d ~ \033[32;49m%s@%d\033[0m %.*s" fmt, getpid(), \
        strrchr (__FILE__, '/') == 0 ?  \
		__FILE__ : strrchr (__FILE__, '/') + 1, \
		__LINE__, \
        wbt_time_str_log.len, wbt_time_str_log.str, \
        ##arg);
#else
#define wbt_log_debug(fmt, arg...) ((void)0);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_LOG_H__ */

