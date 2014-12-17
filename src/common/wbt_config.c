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

wbt_mem_t wbt_config_file_content;

int wbt_conf_line = 1;
int wbt_conf_charactor = 0;

static wbt_status wbt_conf_parse(wbt_mem_t * conf) {
    int status = 0;
    u_char *p, ch;
    
    wbt_conf_line = 1;
    wbt_conf_charactor = 0;
    
    wbt_str_t key, value;
    
    //wbt_log_debug("Parse config file: size %d", conf->len);
    
    int i = 0;
    for( i = 0 ; i < conf->len ; i ++ ) {
        p = (u_char *)conf->ptr;
        ch = *(p+i);

        wbt_conf_charactor ++;

        switch( status ) {
            case 0:
                if( ch == ' ' || ch == '\t' ) {
                    /* continue; */
                } else if( ch == '\n' ) {
                    wbt_conf_line ++;
                    wbt_conf_charactor = 0;
                } else if( ch == '#' ) {
                    status = 1;
                } else if( ( ch >= 'a' && ch <= 'z' ) || ch == '_' ) {
                    key.str = p + i;
                    status = 2;
                } else {
                    return WBT_ERROR;
                }
                break;
            case 1:
                if( ch == '\n' ) {
                    wbt_conf_line ++;
                    wbt_conf_charactor = 0;
                    status = 0;
                }
                break;
            case 2:
                if( ( ch >= 'a' && ch <= 'z' ) || ch == '_' ) {
                    /* continue; */
                } else if( ch == ' ' || ch == '\t' ) {
                    key.len = p + i - key.str;
                    status = 3;
                } else {
                    return WBT_ERROR;
                }
                break;
            case 3:
                if( ch == ' ' || ch == '\t' ) {
                    /* continue; */
                } else {
                    value.str = p + i;
                    status = 4;
                }
                break;
            case 4:
                if( ch == '\n' ) {
                    wbt_conf_line ++;
                    wbt_conf_charactor = 0;
                    value.len = p + i - value.str;
                    status = 0;
                    
                    wbt_log_debug("key: %.*s, value: %.*s", key.len, key.str, value.len, value.str);
                    /* TODO 储存配置信息 */
                }
                break;
            default:
                /* 不应当出现未知的状态 */
                return WBT_ERROR;
        }
    }
    
    return WBT_OK;
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

    wbt_free(&wbt_config_file_content);
    wbt_malloc(&wbt_config_file_content, wbt_config_file.size);
    read(wbt_config_file.fd, wbt_config_file_content.ptr, wbt_config_file_content.len);
    
    /* 关闭配置文件 */
    close(wbt_config_file.fd);
    
    /* 解析配置文件 */
    if( wbt_conf_parse(&wbt_config_file_content) == WBT_OK ) {
        return WBT_OK;
    } else {
        wbt_log_add("Syntax error on config file: line %d, charactor %d\n", wbt_conf_line, wbt_conf_charactor);
        return WBT_ERROR;
    }
}
