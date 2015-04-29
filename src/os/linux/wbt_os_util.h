/* 
 * File:   wbt_os_util.h
 * Author: Fcten
 *
 * Created on 2015年4月29日, 下午4:35
 */

#ifndef __WBT_OS_UTIL_H__
#define	__WBT_OS_UTIL_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include "../../webit.h"
#include "../../common/wbt_string.h"

int wbt_get_file_path_by_fd(int fd, wbt_mem_t* buf);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_OS_UTIL_H__ */

