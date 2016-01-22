/* 
 * File:   wbt_module.h
 * Author: Fcten
 *
 * Created on 2014年10月23日, 下午2:47
 */

#ifndef __WBT_MODULE_H__
#define	__WBT_MODULE_H__

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdio.h>
    
#include "../webit.h"
#include "../http/wbt_http.h"
#include "wbt_string.h"
#include "wbt_event.h"

typedef struct wbt_module_s {
    wbt_str_t   name;
    wbt_status  (*init)();                      /* 模块初始化方法 */
    wbt_status  (*exit)();                      /* 模块卸载方法 */
    wbt_status  (*on_conn)( wbt_event_t * );    /* 连接建立后调用 */
    wbt_status  (*on_recv)( wbt_event_t * );    /* 接收数据后调用 */
    wbt_status  (*on_send)( wbt_event_t * );    /* 需要发送响应时调用 */
    wbt_status  (*on_close)( wbt_event_t * );   /* 关闭连接前调用 */
    wbt_status  (*on_success)( wbt_event_t * ); /* 响应发送成功时调用 */
} wbt_module_t;

wbt_status wbt_module_init();
wbt_status wbt_module_exit();
wbt_status wbt_module_on_conn(wbt_event_t *ev);
wbt_status wbt_module_on_recv(wbt_event_t *ev);
wbt_status wbt_module_on_send(wbt_event_t *ev);
wbt_status wbt_module_on_close(wbt_event_t *ev);
wbt_status wbt_module_on_success(wbt_event_t *ev);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_MODULE_H__ */

