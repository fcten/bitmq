/* 
 * File:   wbt_os_util.c
 * Author: Fcten
 *
 * Created on 2015年4月29日, 下午4:35
 */

#include "../../webit.h"
#include "../../common/wbt_memory.h"
#include "wbt_os_util.h"

int wbt_nonblocking(wbt_socket_t s) {
    unsigned long nb = 1;
    return ioctlsocket(s, FIONBIO, &nb);
}

int wbt_blocking(wbt_socket_t s) {
    unsigned long nb = 0;
    return ioctlsocket(s, FIONBIO, &nb);
}
