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
    wbt_str_t wbt_config_file_name = wbt_string("./webit.conf");
    
    wbt_file_t wbt_config_file;
    wbt_config_file.fd = open(wbt_config_file_name.str, O_RDONLY);
    
    if( wbt_config_file.fd <= 0 ) {
        /* 找不到配置文件 */
        wbt_log_add("Can't find config file: %.*s\n", wbt_config_file_name.len, wbt_config_file_name.str);

        return WBT_ERROR;
    }
    
    struct stat statbuff;  
    if(stat(wbt_config_file_name.str, &statbuff) < 0){
        wbt_config_file.size = 0;  
    }else{  
        wbt_config_file.size = statbuff.st_size;  
    }
    
    wbt_mem_t wbt_config_file_content;
    wbt_malloc(&wbt_config_file_content, wbt_config_file.size);
    read(wbt_config_file.fd, wbt_config_file_content.ptr, wbt_config_file_content.len);
    
    /* 关闭配置文件 */
    close(wbt_config_file.fd);
    
    return WBT_OK;
}
