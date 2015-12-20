/* 
 * File:   wbt_os_util.c
 * Author: Fcten
 *
 * Created on 2015年4月29日, 下午4:35
 */

#include "wbt_os_util.h"

int wbt_get_file_path_by_fd(int fd, wbt_mem_t* buf) {
    if ( fd <= 0 ) {  
        return -1;  
    }  

    char tmp[32] = {'\0'};
    snprintf(tmp, sizeof(tmp), "/proc/self/fd/%d", fd);

    return readlink(tmp, buf->ptr, buf->len);
}

int wbt_getopt(int argc,char * const argv[ ],const char * optstring) {
    return getopt(argc,argv,optstring); 
}  