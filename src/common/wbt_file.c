 /* 
 * File:   wbt_file.c
 * Author: Fcten
 *
 * Created on 2014年11月12日, 上午10:44
 */

#include "wbt_module.h"
#include "wbt_memory.h"
#include "wbt_log.h"
#include "wbt_file.h"
#include "wbt_rbtree.h"

wbt_module_t wbt_module_file = {
    wbt_string("file"),
    wbt_file_init
};

wbt_mem_t wbt_file_path;
wbt_str_t wbt_dir_htdocs = wbt_string("");

wbt_file_t tmp;

#define WBT_MAX_OPEN_FILES 1024

wbt_rbtree_t wbt_file_rbtree;

wbt_status wbt_file_init() {
    wbt_malloc(&wbt_file_path, 256);
    
    wbt_rbtree_init(&wbt_file_rbtree);

    return WBT_OK;
}

wbt_file_t * wbt_file_open( wbt_str_t * file_path ) {
    wbt_str_t full_path;
    /* TODO 需要判断 URI 是否以 / 开头 */
    /* TODO 需要判断被打开的是否为目录，以及文件类型 */
    if( *(file_path->str + file_path->len - 1) == '/' ) {
        full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*sindex.html",
            wbt_dir_htdocs.len, wbt_dir_htdocs.str,
            file_path->len, file_path->str);
    } else {
        full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
            wbt_dir_htdocs.len, wbt_dir_htdocs.str,
            file_path->len, file_path->str);
    }
    
    wbt_log_debug("try to open: %.*s", full_path.len, full_path.str);
    
    tmp.offset = 0;

    if( full_path.len > 255 ) {
        /* 路径过长则拒绝打开 */
        /* 由于接收数据长度存在限制，这里无需担心溢出 */
        wbt_log_debug("file path too long");

        tmp.fd = -1;
    } else {
        wbt_rbtree_node_t *file =  wbt_rbtree_get(&wbt_file_rbtree, &full_path);
        if( file == NULL ) {
            tmp.fd = open(full_path.str, O_RDONLY);

            struct stat statbuff;  
            if(stat(full_path.str, &statbuff) < 0){  
                tmp.size = 0;  
                perror("stat error");
            }else{  
                tmp.size = statbuff.st_size;  
            }
            
            if( tmp.fd > 0 ) {
                file = wbt_rbtree_insert(&wbt_file_rbtree, &full_path);
                file->file.fd = tmp.fd;
                file->file.refer = 1;
                file->file.size = tmp.size;

                wbt_log_debug("open file: %d %d", tmp.fd, tmp.size);
            }
        } else {
            file->file.refer ++;
            
            tmp.fd = file->file.fd;
            tmp.size = file->file.size;
        }
    }

    return &tmp;
}

wbt_status wbt_file_close( wbt_str_t * file_path ) {
    wbt_str_t full_path;
    /* TODO 需要判断 URI 是否以 / 开头 */
    /* TODO 需要判断被打开的是否为目录，以及文件类型 */
    if( *(file_path->str + file_path->len - 1) == '/' ) {
        full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*sindex.html",
            wbt_dir_htdocs.len, wbt_dir_htdocs.str,
            file_path->len, file_path->str);
    } else {
        full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
            wbt_dir_htdocs.len, wbt_dir_htdocs.str,
            file_path->len, file_path->str);
    }

    wbt_rbtree_node_t *file =  wbt_rbtree_get(&wbt_file_rbtree, &full_path);

    if(file != NULL) {
        file->file.refer --;

        if( file->file.refer == 0 ) {
            /* close(file->fd); */
        }
    }

    return WBT_OK;
}