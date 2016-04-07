/* 
 * File:   webit.h
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午3:21
 */

#ifndef __WEBIT_H__
#define	__WEBIT_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#ifndef WIN32
#include "os/linux/wbt_os_util.h"
#else
#include "os/win32/wbt_os_util.h"
#endif

#define WBT_DEBUG

#define WBT_VERSION         "0.5.0"

#define WBT_MAX_EVENTS      512
#define WBT_EVENT_LIST_SIZE 1024
#define WBT_CONN_BACKLOG    511

#define CR      "\r"
#define LF      "\n"
#define CRLF    CR LF

#define WBT_USE_ACCEPT4
    
typedef enum {
    WBT_OK,
    WBT_ERROR
} wbt_status;

typedef int wbt_atomic_t;

void wbt_exit(int exit_code);

#ifdef	__cplusplus
}
#endif

#endif	/* __WEBIT_H__ */

