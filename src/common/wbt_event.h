/* 
 * File:   wbt_event.h
 * Author: Fcten
 *
 * Created on 2014年9月1日, 上午11:13
 */

#ifndef __WBT_EVENT_H__
#define	__WBT_EVENT_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <openssl/ssl.h>

#include "../webit.h"
#include "wbt_memory.h"

typedef struct wbt_event_s {
    int fd;                                         /* 事件句柄 */
    SSL *ssl;                                       /* 使用加密连接 */
    wbt_status (*callback)(struct wbt_event_s *);   /* 超时回调函数 */
    wbt_status (*trigger)(struct wbt_event_s *);    /* 触发回调函数 */
    time_t time_out;                                /* 事件超时时间 */
    unsigned int events;                            /* 事件类型 */
    unsigned int modified;                          /* 事件版本 */
    wbt_mem_t buff;                                 /* 事件数据缓存 */
    wbt_mem_t data;                                 /* 供模块使用的自定义指针 */
} wbt_event_t;

typedef struct wbt_event_pool_s {
    wbt_mem_t pool;                             /* 一次性申请的大内存块 */
    wbt_mem_t available;                        /* 栈，保存可用的内存块 */
    unsigned int max;                           /* 当前可以容纳的事件数 */
    unsigned int top;                           /* 当前栈顶的位置 */
} wbt_event_pool_t; 

/* 初始化事件池 */
wbt_status wbt_event_init();
/* 程序退出前关闭所有未超时的连接 */
wbt_status wbt_event_exit();
/* 添加事件 */
wbt_event_t * wbt_event_add(wbt_event_t *ev);
/* 删除事件 */
wbt_status wbt_event_del(wbt_event_t *ev);
/* 修改事件 */
wbt_status wbt_event_mod(wbt_event_t *ev);
wbt_status wbt_event_cleanup();
/* 事件循环 */
wbt_status wbt_event_dispatch();

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_EVENT_H__ */

