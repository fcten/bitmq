/* 
 * File:   wbt_memory.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午3:50
 */

#include <sys/mman.h> 
#include <malloc.h>
#include "wbt_memory.h"
#include "wbt_config.h"

int wbt_mem_is_oom() { 
    if( wbt_conf.max_memory_usage <= 0 ) {
        return 0;
    }

    struct mallinfo mi = mallinfo();
    //printf("heap_malloc_total=%d heap_free_total=%d heap_in_use=%d\nmmap_total=%d mmap_count=%d\n", 
    //    mi.arena, mi.fordblks, mi.uordblks, 
    //    mi.hblkhd, mi.hblks); 

    if( mi.uordblks > wbt_conf.max_memory_usage ) {
        return 1;
    }
    
    return 0;
}