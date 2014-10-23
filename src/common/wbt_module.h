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
#include "wbt_string.h"

typedef struct wbt_module_s {
    wbt_str_t   name;
    wbt_status  (*init)(/*struct wbt_event_s */);   /* 模块初始化方法 */
} wbt_module_t;


#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_MODULE_H__ */

