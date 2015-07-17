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

typedef struct wbt_module_s {
    wbt_str_t   name;
    wbt_status  (*init)();                      /* 模块初始化方法 */
    wbt_status  (*exit)();                      /* 模块卸载方法 */
    wbt_status  (*on_conn)( wbt_http_t * );
    wbt_status  (*on_recv)( wbt_http_t * );
    wbt_status  (*on_send)( wbt_http_t * );
    wbt_status  (*on_filter)( wbt_http_t * );
} wbt_module_t;

wbt_status wbt_module_init();
wbt_status wbt_module_exit();
wbt_status wbt_module_on_filter(wbt_http_t *http);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_MODULE_H__ */

