/* 
 * File:   webit.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午2:10
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
    p = wbt_malloc(20);
    wbt_memset(p, 0);
    s = wbt_sprintf(p, "Webit startup %d.", getpid());

    wbt_log_write(s, stderr);
    
    wbt_log_debug("123");

    wbt_conn_init();    
    
    return 0;
}

