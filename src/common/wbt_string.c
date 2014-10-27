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

/* 
 * 搜索 str2 字符串在 str1 字符串中第一次出现的位置
 * 大小写敏感
 * 如果没有找到该字符串,则返回 -1
 */
int wbt_strpos( wbt_str_t *str1, wbt_str_t *str2 ) {
    int pos = 0, i = 0, flag = 0;

    for( pos = 0 ; pos <= (str1->len - str2->len) ; pos ++ ) {
        flag = 0;
        for( i = 0 ; i < str2->len ; i ++ ) {
            if( *(str1->str + pos + i) != *(str2->str + i) ) {
                flag = 1;
                break;
            }
        }
        if( !flag ) {
            return pos;
        }
    }
    
    return -1;
}

/*
 * 比较字符串 str1 和 str2
 * 大小写敏感
 * 相同返回 0，否则返回 1
 */
int wbt_strcmp( wbt_str_t *str1, wbt_str_t *str2, int len ) {
    int pos = 0;

    if( len > str1->len || len > str2->len ) {
        return 1;
    }
    
    for( pos = 0; pos < len ; pos ++ ) {
        if( *(str1->str + pos) != *(str2->str + pos) ) {
            return 1;
        }
    }

    return 0;
}