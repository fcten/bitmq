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
    int listen_port;    // 监听端口
    int run_mode;       // 1 - deamon, 2 - debug
    int process;        // 工作进程数量
    int secure;         // 是否启用 https
    wbt_str_t user;     // 低权限用户
    wbt_str_t admin;    // 管理员联系方式
    wbt_str_t root;     // 网站根目录
    wbt_str_t index;    // 访问目录时的默认文件
    wbt_str_t secure_key;  // 私钥文件
    wbt_str_t secure_crt;  // 证书文件
} wbt_conf_t;

extern wbt_conf_t wbt_conf;

wbt_status wbt_conf_set_file( const char * file );

wbt_status wbt_conf_init();
wbt_status wbt_conf_exit();
wbt_status wbt_conf_reload();

const char * wbt_conf_get( const char * name );
wbt_mem_t * wbt_conf_get_v( const char * name );

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_CONFIG_H__ */

