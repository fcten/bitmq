/* 
 * File:   wbt_log.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午4:05
 */

#include "wbt_log.h"
#include "wbt_module.h"
#include "wbt_time.h"

wbt_module_t wbt_module_log = {
    wbt_string("log"),
    wbt_log_init
};

int wbt_log_file_fd;
char * wbt_log_file = "./webit.log";
wbt_mem_t wbt_log_buf;

wbt_status wbt_log_init() {
    wbt_log_file_fd = open(wbt_log_file, O_WRONLY | O_APPEND | O_CREAT);

    if( wbt_malloc( &wbt_log_buf, 1024 ) != WBT_OK ) {        
        return WBT_ERROR;
    }
    
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

wbt_status wbt_log_add(const char *fmt, ...) {
    wbt_str_t s;
    va_list   args;

    s.str = wbt_log_buf.ptr;

    va_start(args, fmt);
    s.len = (size_t) vsnprintf(wbt_log_buf.ptr, wbt_log_buf.len, fmt, args);
    va_end(args);

    /* TODO 先写入缓存，定时写入磁盘 */
    write(wbt_log_file_fd, wbt_time_str_log.str, wbt_time_str_log.len);
    write(wbt_log_file_fd, s.str, s.len);
    
    return WBT_OK;
}