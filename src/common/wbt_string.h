/* 
 * File:   wbt_string.h
 * Author: Fcten
 *
 * Created on 2014年8月18日, 下午3:50
 */

#ifndef __WBT_STRING_H__
#define __WBT_STRING_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

#include "wbt_memory.h"

typedef struct {
    /* 使用 int 而不是 unsigned int，方便进行运算和比较 */
    int len;
    u_char *str;
} wbt_str_t;

typedef struct {
    int len;
    int start;
} wbt_str_offset_t;

typedef union {
    wbt_str_t s;
    wbt_str_offset_t o;
} wbt_str_union_t;

#define wbt_string(str)     { sizeof(str) - 1, (u_char *) str }
#define wbt_null_string     { 0, NULL }

#define wbt_str_set(stri, text)  (stri)->len = sizeof(text) - 1; (stri)->str = (u_char *) text
#define wbt_str_set_null(stri)   (stri)->len = 0; (stri)->str = NULL

#define wbt_offset_to_str(offset, stri, p)   do { \
    (stri).str = (char *)(p) + (offset).start; \
    (stri).len = (offset).len; \
} while(0);

const char * wbt_stdstr(wbt_str_t * str);

wbt_str_t wbt_sprintf(wbt_mem_t *buf, const char *fmt, ...);

int wbt_strpos( wbt_str_t *str1, wbt_str_t *str2 );
int wbt_strcmp( wbt_str_t *str1, wbt_str_t *str2, int len );
int wbt_stricmp( wbt_str_t *str1, wbt_str_t *str2, int len );
int wbt_strcmp2( wbt_str_t *str1, wbt_str_t *str2);

inline void wbt_strcat( wbt_str_t * dest, wbt_str_t * src, int max_len );
inline unsigned int wbt_strlen(const char *s);

inline int wbt_atoi(wbt_str_t * str);

#ifdef	__cplusplus
}
#endif

#endif /* __WBT_STRING_H__ */
