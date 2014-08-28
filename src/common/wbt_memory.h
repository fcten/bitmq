/* 
 * File:   wbt_memory.h
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午3:50
 */

#ifndef __WBT_MEMORY_H__
#define	__WBT_MEMORY_H__


#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t  len;
    void   *ptr;
} wbt_mem_t;

inline wbt_mem_t wbt_malloc(size_t len);
inline void wbt_free(wbt_mem_t p);
inline void wbt_memset(wbt_mem_t p, int ch);


#endif	/* __WBT_MEMORY_H__ */

