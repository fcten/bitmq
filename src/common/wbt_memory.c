/* 
 * File:   wbt_memory.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午3:50
 */

#include "wbt_memory.h"

inline wbt_status wbt_malloc(wbt_mem_t *p, size_t len) {
    p->ptr = malloc(len);
    if(p->ptr != NULL) {
        p->len = len;
        p->next = NULL;
        
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

inline wbt_status wbt_realloc(wbt_mem_t *p, size_t len) {
    void *tmp = realloc(p->ptr, len);
    if(tmp != NULL) {
        p->ptr = tmp;
        p->len = len;
        
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

inline void wbt_free(wbt_mem_t *p) {
    if( p->len > 0 && p->ptr != NULL ) {
        free(p->ptr);
        p->ptr = NULL;
        p->len = 0;
    }
}

inline void wbt_memset(wbt_mem_t *p, int ch) {
    memset(p->ptr, ch, p->len);
}

inline void wbt_memcpy(wbt_mem_t *dest, wbt_mem_t *src, size_t len) {
    if( dest->len >= len ) {
        memcpy( dest->ptr, src->ptr, len );
    } else {
        memcpy( dest->ptr, src->ptr, dest->len );
    }
}