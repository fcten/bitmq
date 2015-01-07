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

#define WBT_DEBUG

#define WBT_MAX_EVENTS      512
#define WBT_EVENT_LIST_SIZE 1024
#define WBT_CONN_BACKLOG    511
#define WBT_CONN_TIMEOUT    15000    /* keep-alive 保持时间，单位毫秒 */

#define CR      "\r"
#define LF      "\n"
#define CRLF    CR LF
    
#define WBT_USE_ACCEPT4
    
typedef enum {
    WBT_OK,
    WBT_ERROR
} wbt_status;

#ifdef	__cplusplus
}
#endif

#endif	/* __WEBIT_H__ */

