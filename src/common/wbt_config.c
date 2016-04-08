/* 
 * File:   wbt_config.c
 * Author: Fcten
 *
 * Created on 2014年12月16日, 下午3:03
 */

#include "wbt_config.h"
#include "wbt_module.h"
#include "wbt_file.h"
#include "wbt_log.h"
#include "wbt_rbtree.h"
#include "../os/linux/wbt_os_util.h"
#include "../os/linux/wbt_setproctitle.h"

wbt_module_t wbt_module_conf = {
    wbt_string("config"),
    wbt_conf_init,
    wbt_conf_exit
};

/* 默认的配置文件位置 */
wbt_str_t wbt_config_file_name = wbt_string("./webit.conf");

wbt_str_t wbt_conf_option_on = wbt_string("on");
wbt_str_t wbt_conf_option_off = wbt_string("off");

wbt_str_t wbt_conf_option_always = wbt_string("always");
wbt_str_t wbt_conf_option_everysec = wbt_string("everysec");

/* 供 setproctitle 显示使用，如果路径过长则会被截断 */
wbt_str_t wbt_config_file_path;
wbt_str_t wbt_config_file_content;

wbt_rb_t wbt_config_rbtree;

int wbt_conf_line = 1;
int wbt_conf_charactor = 0;

wbt_conf_t wbt_conf;

wbt_status wbt_conf_init() {
    wbt_rb_init(&wbt_config_rbtree, WBT_RB_KEY_STRING);
    
    wbt_config_file_path.len = 64;
    wbt_config_file_path.str = wbt_malloc( wbt_config_file_path.len );
    if( wbt_config_file_path.str == NULL ) {
        return WBT_ERROR;
    }
    
    if( wbt_conf_reload() != WBT_OK ) {
        return WBT_ERROR;
    }
    
    /* 初始化 wbt_conf */
    const char * value;
    wbt_str_t * m_value;
    
    wbt_conf.listen_port = 80;
    if( ( value = wbt_conf_get("listen") ) != NULL ) {
        wbt_conf.listen_port = atoi(value);
        if( wbt_conf.listen_port < 0 || wbt_conf.listen_port > 65535  ) {
            wbt_log_add("listen port out of range ( expect 0 - 65535 )\n");
            return WBT_ERROR;
        }
    }
    
    wbt_conf.process = 1;
    if( ( value = wbt_conf_get("process") ) != NULL ) {
        wbt_conf.process = atoi(value);
        if( wbt_conf.process < 1 || wbt_conf.process > 128 ) {
            wbt_log_add("worker process number out of range ( expect 1 - 128 )\n");
            return WBT_ERROR;
        }
    }
    
    wbt_conf.secure = 0;
    if( ( m_value = wbt_conf_get_v("secure") ) != NULL ) {
        if( wbt_strcmp( m_value, &wbt_conf_option_on ) == 0 ) {
            wbt_conf.secure = 1;
        }
    }
    
    wbt_str_set_null(wbt_conf.secure_key);
    wbt_str_set_null(wbt_conf.secure_crt);
    if( wbt_conf.secure ) {
        if( ( m_value = wbt_conf_get_v("secure_key") ) != NULL ) {
            wbt_conf.secure_key = *m_value;
        } else {
            wbt_log_add("option secure_key is required\n");
            return WBT_ERROR;
        }
        
        if( ( m_value = wbt_conf_get_v("secure_crt") ) != NULL ) {
            wbt_conf.secure_crt = *m_value;
        } else {
            wbt_log_add("option secure_crt is required\n");
            return WBT_ERROR;
        }
    }

    wbt_conf.sendfile = 0;
    if( !wbt_conf.secure ) {
        if( ( m_value = wbt_conf_get_v("sendfile") ) != NULL ) {
            if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_on ) == 0 ) {
                wbt_conf.sendfile = 1;
            }
        }
    }

    wbt_conf.gzip = 0;
    if( ( m_value = wbt_conf_get_v("gzip") ) != NULL ) {
        if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_on ) == 0 ) {
            wbt_conf.gzip = 1;
        }
    }

    wbt_conf.aof = 1;
    if( ( m_value = wbt_conf_get_v("aof") ) != NULL ) {
        if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_off ) == 0 ) {
            wbt_conf.aof = 0;
        }
    }

    wbt_conf.aof_crc = 0;
    if( ( m_value = wbt_conf_get_v("aof_crc") ) != NULL ) {
        if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_on ) == 0 ) {
            wbt_conf.aof_crc = 1;
        }
    }

    wbt_conf.aof_fsync = AOF_FSYNC_EVERYSEC;
    if( ( m_value = wbt_conf_get_v("aof_fsync") ) != NULL ) {
        if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_off ) == 0 ) {
            wbt_conf.aof_fsync = AOF_FSYNC_NO;
        } else if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_always ) == 0 ) {
            wbt_conf.aof_fsync = AOF_FSYNC_ALWAYS;
        } else if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_everysec ) == 0 ) {
            wbt_conf.aof_fsync = AOF_FSYNC_EVERYSEC;
        } else {
            wbt_log_add("option aof_fsync is illegal ( expect off / always / everysec )\n");
            return WBT_ERROR;
        }
    }

    wbt_conf.aof_fast_boot = 1;
    if( ( m_value = wbt_conf_get_v("aof_fast_boot") ) != NULL ) {
        if( wbt_strcmp( (wbt_str_t *)m_value, &wbt_conf_option_off ) == 0 ) {
            wbt_conf.aof_fast_boot = 0;
        }
    }

    wbt_conf.keep_alive_timeout = 600000;
    if( ( value = wbt_conf_get("keep_alive_timeout") ) != NULL ) {
        wbt_conf.keep_alive_timeout = atoi(value);
    }
    
    wbt_conf.event_timeout = 150000;
    if( ( value = wbt_conf_get("event_timeout") ) != NULL ) {
        wbt_conf.event_timeout = atoi(value);
    }

    wbt_conf.max_open_files = 65535;
    if( ( value = wbt_conf_get("max_open_files") ) != NULL ) {
        wbt_conf.max_open_files = atoi(value);
    }
    
    wbt_conf.max_core_file_size = 0;
    if( ( value = wbt_conf_get("max_core_file_size") ) != NULL ) {
        wbt_conf.max_core_file_size = atoi(value);
    }
    
    wbt_conf.max_memory_usage = 0;
    if( ( value = wbt_conf_get("max_memory_usage") ) != NULL ) {
        wbt_conf.max_memory_usage = 1024*1024*atoll(value);
    }

    wbt_str_set_null(wbt_conf.root); 
    if( ( m_value = wbt_conf_get_v("root") ) != NULL ) {
        wbt_conf.root = *m_value;
        // TODO 检查 root 是否存在
    } else {
        wbt_log_add("option root is required\n");
        return WBT_ERROR;
    }
    
    wbt_str_set_null(wbt_conf.index);
    if( ( m_value = wbt_conf_get_v("default") ) != NULL ) {
        wbt_conf.index = *m_value;
    }

    wbt_str_set_null(wbt_conf.admin);
    if( ( m_value = wbt_conf_get_v("server_admin") ) != NULL ) {
        wbt_conf.admin = *m_value;
    }

    wbt_str_set_null(wbt_conf.user);
    if( ( m_value = wbt_conf_get_v("user") ) != NULL ) {
        wbt_conf.user = *m_value;
    }

    return WBT_OK;
}

wbt_status wbt_conf_exit() {
    return WBT_OK;
}

wbt_status wbt_conf_set_file( const char * file ) {
    wbt_config_file_name.str = (char *)file; /* 这个类型转换只是为了避免编译器报
                                              * 错，不允许通过该指针进行写操作 */
    wbt_config_file_name.len = wbt_strlen(file);
    
    return WBT_OK;
}

static wbt_status wbt_conf_parse(wbt_str_t * conf) {
    int status = 0;
    char *p, ch;
    
    wbt_conf_line = 1;
    wbt_conf_charactor = 0;
    
    wbt_str_t key, value;
    
    //wbt_log_debug("Parse config file: size %d", conf->len);
    
    int i = 0;
    for( i = 0 ; i < conf->len ; i ++ ) {
        p = conf->str;
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
                    
                    /* 储存配置信息 */
                    wbt_rb_node_t *option =  wbt_rb_get(&wbt_config_rbtree, &key);
                    if( option == NULL ) {
                        /* 新的值 */
                        option = wbt_rb_insert(&wbt_config_rbtree, &key);
                    } else {
                        /* 已有的值 */
                        wbt_free(option->value.str);
                        option->value.len = 0;
                    }
                    option->value.str = wbt_strdup(value.str, value.len);
                    if( option->value.str == NULL ) {
                        /* 这里返回 WBT_ERROR 会导致提示配置文件语法错误，
                         * 但事实上是由于内存不足导致的。在启动阶段出现这种情况的概率不大，所以就这样吧。 */
                        return WBT_ERROR;
                    }
                    option->value.len = value.len;
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
    /* TODO 清理已保存的配置信息 */

    /* 尝试读取配置文件 */
    wbt_file_t wbt_config_file;
	wbt_config_file.fd = wbt_open_file(wbt_config_file_name.str);
    
    if( wbt_config_file.fd <= 0 ) {
        /* 找不到配置文件 */
        wbt_log_add("Can't find config file: %.*s\n", wbt_config_file_name.len, wbt_config_file_name.str);
        return WBT_ERROR;
    }

#ifndef WIN32
    int len = wbt_get_file_path_by_fd(wbt_config_file.fd, wbt_config_file_path.str, wbt_config_file_path.len);
    if( len <= 0 ) {
        return WBT_ERROR;
    }
    wbt_config_file_path.str[len] = '\0';
#endif
    
    struct stat statbuff;  
    if(stat(wbt_config_file_name.str, &statbuff) < 0){
        return WBT_ERROR;
    }
    wbt_config_file.size = statbuff.st_size;

    wbt_config_file_content.len = wbt_config_file.size;
    wbt_config_file_content.str = wbt_malloc( wbt_config_file_content.len );
    if( wbt_config_file_content.str == NULL ) {
        return WBT_ERROR;
    }
    if( wbt_read_file( wbt_config_file.fd, wbt_config_file_content.str, wbt_config_file_content.len, 0) != wbt_config_file_content.len ) {
        wbt_log_add("Read config file failed\n");
        return WBT_ERROR;
    }
    
    /* 关闭配置文件 */
	wbt_close_file(wbt_config_file.fd);

    /* 解析配置文件 */
    if( wbt_conf_parse(&wbt_config_file_content) == WBT_OK ) {
        //wbt_rbtree_print(wbt_config_rbtree.root);
        wbt_free(wbt_config_file_content.str);

#ifndef WIN32
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "webit: master process (%.*s)", len, wbt_config_file_path.str );
        wbt_set_proc_title(tmp);
#endif

		return WBT_OK;
    } else {
        wbt_free(wbt_config_file_content.str);
        wbt_log_add("Syntax error on config file: line %d, charactor %d\n", wbt_conf_line, wbt_conf_charactor);
        return WBT_ERROR;
    }
}

const char * wbt_conf_get( const char * name ) {
    wbt_str_t * tmp = wbt_conf_get_v( name );
    if( tmp == NULL ) {
        return NULL;
    } else {
        return wbt_stdstr( tmp );
    }
}

wbt_str_t * wbt_conf_get_v( const char * name ) {
    wbt_str_t config_name;
    config_name.str = (char *)name;
    config_name.len = strlen(name);
    wbt_rb_node_t * root = wbt_rb_get(&wbt_config_rbtree, &config_name);
    if( root == NULL ) {
        return NULL;
    } else {
        return &root->value;
    }
}
