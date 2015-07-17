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

#ifdef	__cplusplus
extern "C" {
#endif

wbt_status wbt_module_helloworld_filter(wbt_http_t *http);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MODULE_HELLOWORLD_H */

