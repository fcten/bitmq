/* 
 * File:   wbt_service.h
 * Author: Fcten
 *
 * Created on 2016年4月10日, 下午9:05
 */

#ifndef __WBT_SERVICE_H__
#define	__WBT_SERVICE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <windows.h>

void service_install();
void service_uninstall();
void service_run();

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_SERVICE_H__ */

