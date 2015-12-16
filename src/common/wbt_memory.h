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
    struct wbt_mem_s *next; /* 指向下一个内存块 TODO 移除该属性 */
} wbt_mem_t;

inline wbt_status wbt_malloc(wbt_mem_t * p, size_t len);
inline wbt_status wbt_calloc(wbt_mem_t * p, size_t len, size_t size);
inline void wbt_free(wbt_mem_t * p);
inline void wbt_memset(wbt_mem_t * p, int ch);
inline wbt_status wbt_realloc(wbt_mem_t * p, size_t len);
inline void wbt_memcpy(wbt_mem_t *dest, wbt_mem_t *src, size_t len);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_MEMORY_H__ */

