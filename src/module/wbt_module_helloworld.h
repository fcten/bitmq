/* 
 * File:   wbt_module_helloworld.h
 * Author: fcten
 *
 * Created on 2015年7月17日, 上午10:39
 */

#ifndef WBT_MODULE_HELLOWORLD_H
#define	WBT_MODULE_HELLOWORLD_H

#include "../webit.h"
#include "../http/wbt_http.h"
#include "../common/wbt_module.h"

#ifdef	__cplusplus
extern "C" {
#endif

wbt_status wbt_module_helloworld_init();
wbt_status wbt_module_helloworld_conn(wbt_event_t *ev);
wbt_status wbt_module_helloworld_recv(wbt_event_t *ev);
wbt_status wbt_module_helloworld_close(wbt_event_t *ev);

wbt_status wbt_module_helloworld_login(wbt_event_t *ev);
wbt_status wbt_module_helloworld_show(wbt_event_t *ev);

typedef struct wbt_online_s {
    /* 连接状态 */
    int status;
    /* 上一次上线时间 */
    time_t online;
    /* 上一次离线时间 */
    time_t offline;
    /* 连接句柄 */
    wbt_event_t *ev;
    /* 客户端 ID */
    char id[32];
} wbt_online_t;

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MODULE_HELLOWORLD_H */

