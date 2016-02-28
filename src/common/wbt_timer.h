/* 
 * File:   wbt_timer.h
 * Author: fcten
 *
 * Created on 2016年2月28日, 下午6:33
 */

#ifndef WBT_TIMER_H
#define	WBT_TIMER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "wbt_time.h"

typedef struct wbt_timer_s {
    wbt_status (*on_timeout)(struct wbt_timer_s *); /* 超时回调函数 */
    time_t timeout;                                 /* 事件超时时间 */
    unsigned int heap_idx;                          /* 在超时队列中的位置 */
} wbt_timer_t;

/**
 * 用于通过 wbt_timer_t 指针获得完整结构体的指针
 * @ptr:                wbt_timer_t 结构体指针
 * @type:               想要获取的完整结构体类型
 * @member:         wbt_timer_t 成员在完整结构体中的名称
 */
#define wbt_timer_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

//typedef struct wbt_event_s {
//    wbt_status (*on_recv)(struct wbt_event_s *);    /* 触发回调函数 */
//    wbt_status (*on_send)(struct wbt_event_s *);    /* 触发回调函数 */
//    unsigned int events;                            /* 事件类型 */
//} wbt_event_t;
//
//typedef struct wbt_conn_s {
//    int fd;                                         /* 事件句柄 */
//    SSL * ssl;                                      /* 使用加密连接 */
//    void * buff;                                    /* 事件数据缓存（接收到的数据） */
//    size_t buff_len;
//    void * data;                                    /* 供模块使用的自定义指针（http结构体） */
//    void * ctx;
//    wbt_event_t event;
//    wbt_timer_t timer;
//} wbt_conn_t;

wbt_status wbt_timer_init();
wbt_status wbt_timer_exit();

wbt_status wbt_timer_add(wbt_timer_t *timer);
wbt_status wbt_timer_del(wbt_timer_t *timer);
wbt_status wbt_timer_mod(wbt_timer_t *timer);

time_t wbt_timer_process();

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_TIMER_H */

