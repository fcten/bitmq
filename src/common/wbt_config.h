/* 
 * File:   wbt_config.h
 * Author: Fcten
 *
 * Created on 2014年12月16日, 下午3:03
 */

#ifndef __WBT_CONFIG_H__
#define	__WBT_CONFIG_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_string.h"
#include "wbt_module.h"

wbt_status wbt_conf_init();

wbt_status wbt_conf_reload();

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_CONFIG_H__ */

