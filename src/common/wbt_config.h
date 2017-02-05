/* 
 * File:   wbt_config.h
 * Author: Fcten
 *
 * Created on 2014年12月16日, 下午3:03
 */

#ifndef __WBT_CONFIG_H__
#define	__WBT_CONFIG_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "wbt_string.h"

typedef struct wbt_conf_s {
    // 监听端口
    int listen_port;
    // 工作进程数量
    int process;
    // 是否使用后台模式
    int daemon;
    // 是否启用 TLS
    int secure;
    // 是否使用 sendfile
    int sendfile;
    // 是否使用 gzip
    int gzip;
    // 是否启用持久化
    int aof;
    // 是否使用 crc 校验
    int aof_crc;
    // 刷盘策略
    int aof_fsync;
    // 是否使用快速启动
    int aof_fast_boot;
    // 长连接超时时间 毫秒
    int keep_alive_timeout;
    // 事件超时时间 毫秒
    int event_timeout;
    // 最多打开的句柄数量
    int max_open_files;
    // 最大的 core dump 文件大小
    int max_core_file_size;
    // 最大内存使用
    long long int max_memory_usage;
    // 低权限用户
    wbt_str_t user;
    // 管理员联系方式
    wbt_str_t admin;
    // 网站根目录
    wbt_str_t root;
    // 访问目录时的默认文件
    wbt_str_t index;
    // 私钥文件路径
    wbt_str_t secure_key;
    // 证书文件路径
    wbt_str_t secure_crt;
} wbt_conf_t;

#define AOF_FSYNC_NO       0
#define AOF_FSYNC_ALWAYS   1
#define AOF_FSYNC_EVERYSEC 2

#define WBT_MAX_PROTO_BUF_LEN 64*1024

extern wbt_conf_t wbt_conf;

wbt_status wbt_conf_set_file( const char * file );

wbt_status wbt_conf_init();
wbt_status wbt_conf_exit();
wbt_status wbt_conf_reload();

const char * wbt_conf_get( const char * name );
wbt_str_t * wbt_conf_get_v( const char * name );

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_CONFIG_H__ */

