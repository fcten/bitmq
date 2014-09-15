/* 
 * File:   webit.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午2:10
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>

#include "webit.h"
#include "common/wbt_string.h"
#include "common/wbt_memory.h"
#include "common/wbt_log.h"

/*
 * 
 */
int main(int argc, char** argv) {
    
    wbt_mem_t p;
    wbt_str_t s;
    wbt_malloc(&p, 20);
    wbt_memset(&p, 0);
    s = wbt_sprintf(&p, "Webit startup %d.", getpid());
    wbt_log_write(s, stderr);
    wbt_free(&p);
    
    wbt_log_debug("123");

    if( wbt_event_init() != WBT_OK ) {
        return 1;
    }

    if( wbt_conn_init() != WBT_OK ) {
        return 1;
    }
    
    /* 设置程序允许打开的最大文件句柄数 */
    struct rlimit rlim;
    rlim.rlim_cur = 65535;
    rlim.rlim_max = 65535;
    setrlimit(RLIMIT_NOFILE, &rlim);
    
    wbt_event_dispatch();
    
    return 0;
}

