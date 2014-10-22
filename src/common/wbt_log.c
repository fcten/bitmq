/* 
 * File:   wbt_log.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午4:05
 */

#include "wbt_log.h"

int wbt_log_write(wbt_str_t p, FILE *fp) {
    time_t     now;
    struct tm *timenow;
    char tmpbuf[32];

    time(&now);
    timenow = localtime(&now);
    
    fputs("[\033[32;49m", fp);
    strftime(tmpbuf, 32, "%F %T", timenow);
    fputs(tmpbuf, fp);
    fputs("\033[0m] ", fp);
    fputs(p.str, fp);
    if(errno != 0 && fp == stderr) {
        fputs(": ", fp);
        fputs(strerror(errno), fp);
    }
    fputs("\n", fp);
    
    return 0;
}