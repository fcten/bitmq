/* 
 * File:   wbt_memory.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午3:50
 */

#include "wbt_memory.h"

inline wbt_mem_t wbt_malloc(size_t len) {
    wbt_mem_t p;
    
    p.ptr = malloc(len);
    p.len = len;
    
    return p;
}

inline void wbt_free(wbt_mem_t p) {
    free(p.ptr);
}

inline void wbt_memset(wbt_mem_t p, int ch) {
    memset(p.ptr, ch, p.len);
}