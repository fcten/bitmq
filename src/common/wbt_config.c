/* 
 * File:   wbt_config.c
 * Author: Fcten
 *
 * Created on 2014年12月16日, 下午3:03
 */

#include "wbt_config.h"

wbt_module_t wbt_module_conf = {
    wbt_string("config"),
    wbt_conf_init
};

wbt_status wbt_conf_init() {
    return wbt_conf_reload();
}

wbt_status wbt_conf_reload() {
    wbt_str_t wbt_config_file = wbt_string("./webit.conf");
    
    int wbt_config_file_fd = open(wbt_config_file.str, O_RDONLY);
    
    if( wbt_config_file_fd <= 0 ) {
        /* 找不到配置文件 */
        wbt_log_add("Can't find config file: %.*s\n", wbt_config_file.len, wbt_config_file.str);

        return WBT_ERROR;
    }
    
    
    
    /* 关闭配置文件 */
    close(wbt_config_file_fd);
    
    return WBT_OK;
}
