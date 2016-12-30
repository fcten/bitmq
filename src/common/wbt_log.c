/* 
 * File:   wbt_log.c
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午4:05
 */

#include "wbt_log.h"
#include "wbt_module.h"

wbt_module_t wbt_module_log = {
    wbt_string("log"),
    wbt_log_init
};

wbt_fd_t wbt_log_file_fd;
ssize_t wbt_log_file_size;
char * wbt_log_file = "./logs/webit.log";
wbt_str_t wbt_log_buf;

wbt_status wbt_log_init() {
    /* O_CLOEXEC 需要 Linux 内核版本大于等于 2.6.23 */
    /* TODO 每个 worker 进程需要单独的日志文件，否则并发写会丢失一部分数据 */
	wbt_log_file_fd = wbt_open_logfile(wbt_log_file);
    if( (int)wbt_log_file_fd <=0 ) {
		wbt_log_debug("can't open log file, errno: %d\n", wbt_errno);
        return WBT_ERROR;
    }
    
    // 获取日志文件大小
    wbt_log_file_size = wbt_get_file_size( wbt_log_file_fd );
    if( wbt_log_file_size < 0 ) {
		wbt_log_debug("can't get log file size, errno: %d\n", wbt_errno);
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

// 自动分割日志文件
wbt_status wbt_log_rotate() {
    // 重命名旧的日志文件
    char new_log_file[64];
    snprintf( new_log_file, sizeof(new_log_file), "./logs/webit_%ld.log", wbt_cur_mtime );

#ifdef WIN32
    if(wbt_close_file(wbt_log_file_fd) < 0) {
        return WBT_ERROR;
    }
    wbt_log_file_fd = 0;

    if(MoveFileEx(wbt_log_file, new_log_file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        // 尝试重新打开日志文件
	    wbt_log_file_fd = wbt_open_logfile(wbt_log_file);
        if( (int)wbt_log_file_fd <=0 ) {
            wbt_log_file_fd = 0;
		    wbt_log_debug("can't open log file, errno: %d\n", wbt_errno);
        }

        return WBT_ERROR;
    }
#else
    if( rename(wbt_log_file, new_log_file) < 0 ) {
        wbt_log_add("Could not rename log file. "
            "rename: %d\n", wbt_errno);

        return WBT_ERROR;
    }
#endif

    // 创建新的日志文件
	wbt_log_file_fd = wbt_open_logfile(wbt_log_file);
    if( (int)wbt_log_file_fd <=0 ) {
        wbt_log_file_fd = 0;
		wbt_log_debug("can't open log file, errno: %d\n", wbt_errno);
        return WBT_ERROR;
    }
    
    // 可选：压缩旧的日志文件
    
    // 可选：清理旧的日志文件
    
    return WBT_OK;
}

wbt_status wbt_log_add(const char *fmt, ...) {
    wbt_str_t s;
    va_list   args;

    s.str = wbt_log_buf.str;

    va_start(args, fmt);
    s.len = (size_t) vsnprintf(wbt_log_buf.str, wbt_log_buf.len, fmt, args);
    va_end(args);

    /* 操作系统本身会对写入操作进行缓存
     * 需要考虑的是是否需要将日志强制刷入磁盘
     */
	wbt_append_file(wbt_log_file_fd, wbt_time_str_log.str, wbt_time_str_log.len);
	wbt_append_file(wbt_log_file_fd, s.str, s.len);
	
	// 当日志过大时，分割日志文件
	wbt_log_file_size += ( wbt_time_str_log.len + s.len );
    // TODO 可配置日志文件大小
	if( wbt_log_file_size >= 128 * 1024 * 1024 ) {
	    if( wbt_log_rotate() == WBT_OK ) {
	        wbt_log_file_size = 0;
	    }
	}
    
    return WBT_OK;
}

wbt_status wbt_log_print(const char *fmt, ...) {
    wbt_str_t s;
    va_list   args;

    if( (s.str = wbt_log_buf.str) == NULL ) {
        return WBT_ERROR;
    }

    va_start(args, fmt);
    s.len = (size_t) vsnprintf(wbt_log_buf.str, wbt_log_buf.len, fmt, args);
    va_end(args);

    fprintf(stdout, "%.*s", s.len, s.str);
    
    return WBT_OK;
}
