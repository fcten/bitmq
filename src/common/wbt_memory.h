/* 
 * File:   wbt_memory.h
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午3:50
 */

#ifndef __WBT_MEMORY_H__
#define	__WBT_MEMORY_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#include "../webit.h"

typedef struct wbt_mem_s {
    unsigned int  len;            /* 内存块的大小 */
    void   *ptr;            /* 指向内存块的指针 */
} wbt_mem_t;

static inline void * wbt_new(size_t len) {
    return malloc(len);
}

#define wbt_new(type) calloc(1, sizeof(type))

#define wbt_delete(ptr) free(ptr)

static inline wbt_status wbt_malloc(wbt_mem_t *p, size_t len) {
    p->ptr = malloc(len);
    if(p->ptr != NULL) {
        p->len = len;
        
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

static inline wbt_status wbt_calloc(wbt_mem_t *p, size_t len, size_t size) {
    p->ptr = calloc(len, size);
    if(p->ptr != NULL) {
        p->len = len;
        
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

static inline wbt_status wbt_realloc(wbt_mem_t *p, size_t len) {
    void *tmp = realloc(p->ptr, len);
    /* bugfix: len == 0 时相当于执行 free，此时 tmp == NULL */
    if(tmp != NULL || len == 0) {
        p->ptr = tmp;
        p->len = len;
        
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

static inline void wbt_free(wbt_mem_t *p) {
    if( p->len > 0 && p->ptr != NULL ) {
        free(p->ptr);
        p->ptr = NULL;
        p->len = 0;
    }
}

static inline void wbt_memset(wbt_mem_t *p, int ch) {
    memset(p->ptr, ch, p->len);
}

static inline void wbt_memcpy(wbt_mem_t *dest, wbt_mem_t *src, size_t len) {
    if( dest == NULL ) return;
    if( src == NULL ) {
        if( dest->len >= len ) {
            memset( dest->ptr, '\0', len);
        } else {
            memset( dest->ptr, '\0', dest->len);
        }
    } else {
        if( dest->len >= len ) {
            memcpy( dest->ptr, src->ptr, len );
        } else {
            memcpy( dest->ptr, src->ptr, dest->len );
        }
    }
}

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_MEMORY_H__ */

