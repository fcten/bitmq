/* 
 * File:   wbt_log.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午4:05
 */

#include <fcntl.h>

#include "wbt_log.h"

int wbt_log_file_fd;
char * wbt_log_file = "/home/wwwroot/webit.log";

wbt_status wbt_log_init() {
    wbt_log_file_fd = open(wbt_log_file, O_WRONLY | O_APPEND | O_CREAT);
    
    return WBT_OK;
}

wbt_status wbt_log_write(wbt_str_t p) {
    time_t     now;
    struct tm *timenow;
    char tmpbuf[32];
    
    int fp = wbt_log_file_fd;

    time(&now);
    timenow = localtime(&now);
    
    write(fp, "[", 1);
    strftime(tmpbuf, 32, "%F %T", timenow);
    write(fp, tmpbuf, strlen(tmpbuf));
    write(fp, "] ", 2);
    write(fp, p.str, p.len);
    if(errno != 0) {
        write(fp, ": ", 2);
        write(fp, strerror(errno), strlen(strerror(errno)));
    }
    write(fp, "\n", 1);

#ifdef WBT_DEBUG
    fputs("[\033[32;49m", stderr);
    fputs(tmpbuf, stderr);
    fputs("\033[0m] ", stderr);
    fputs(p.str, stderr);
    if(errno != 0) {
        fputs(": ", stderr);
        fputs(strerror(errno), stderr);
    }
    fputs("\n", stderr);
#endif
    
    return WBT_OK;
}