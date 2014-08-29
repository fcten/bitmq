/* 
 * File:   wbt_string.c
 * Author: Fcten
 *
 * Created on 2014年8月18日, 下午3:50
 */

#include "wbt_string.h"

wbt_str_t wbt_sprintf(wbt_mem_t *buf, const char *fmt, ...) {
    wbt_str_t    p;
    va_list   args;
    
    p.str = buf->ptr;

    va_start(args, fmt);
    p.len = (size_t) vsnprintf(buf->ptr, buf->len, fmt, args);
    va_end(args);

    return p;
}