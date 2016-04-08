/* 
 * File:   wbt_setproctitle.h
 * Author: Fcten
 *
 * Created on 2015年4月20日, 下午1:08
 */

#ifndef __WBT_SETPROCTITLE_H__
#define	__WBT_SETPROCTITLE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../webit.h"

int wbt_init_proc_title();
void wbt_set_proc_title(const char *title);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_SETPROCTITLE_H__ */

