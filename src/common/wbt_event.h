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

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include <openssl/ssl.h>

#include "../webit.h"
#include "wbt_list.h"
#include "wbt_timer.h"

enum {
    WBT_PROTOCOL_UNKNOWN,
    WBT_PROTOCOL_HTTP,
    WBT_PROTOCOL_BMTP,
    WBT_PROTOCOL_LENGTH
};

typedef struct wbt_event_s {
    int fd;                                         /* 事件句柄 */
    unsigned int events;                            /* 事件类型 */
    wbt_status (*on_recv)(struct wbt_event_s *);    /* 触发回调函数 */
    wbt_status (*on_send)(struct wbt_event_s *);    /* 触发回调函数 */
    wbt_timer_t timer;
    SSL * ssl;                                      /* 使用加密连接 */
    void * buff;                                    /* 事件数据缓存（接收到的数据） */
    size_t buff_len;
    int protocol;
    void * data;                                    /* 供模块使用的自定义指针（http结构体） */
    void * ctx;
} wbt_event_t;

typedef struct wbt_event_pool_node_s {
    wbt_event_t * pool;
    wbt_list_t list;
} wbt_event_pool_node_t;

typedef struct wbt_event_pool_s {
    wbt_event_pool_node_t node;                 /* 一次性申请的大内存块 */
    wbt_event_t ** available;                   /* 栈，保存可用的内存块 */
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

