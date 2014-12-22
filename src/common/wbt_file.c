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

wbt_file_t tmp;

#define WBT_MAX_OPEN_FILES 1024

wbt_rbtree_t wbt_file_rbtree;

wbt_status wbt_file_init() {
    wbt_rbtree_init(&wbt_file_rbtree);

    return WBT_OK;
}

wbt_file_t * wbt_file_open( wbt_str_t * file_path ) {    
    wbt_log_debug("try to open: %.*s", file_path->len, file_path->str);
    
    tmp.offset = 0;

    if( file_path->len >= 512 ) {
        /* 路径过长则拒绝打开 */
        /* 由于接收数据长度存在限制，这里无需担心溢出 */
        wbt_log_debug("file path too long");

        tmp.fd = -1;
    } else {
        wbt_rbtree_node_t *file =  wbt_rbtree_get(&wbt_file_rbtree, file_path);
        if( file == NULL ) {
            struct stat statbuff;  
            if(stat(file_path->str, &statbuff) < 0){  
                tmp.size = 0;
                tmp.fd   = -1;
                wbt_log_debug("stat error, %.*s not exists.", file_path->len, file_path->str);
            }else{  
                if( S_ISDIR(statbuff.st_mode) ) {
                     /* 尝试打开的是目录 */
                    tmp.size = 0;
                    tmp.fd   = -1;
                } else {
                    /* 尝试打开的是文件 */
                    tmp.size = statbuff.st_size;
                    tmp.fd = open(file_path->str, O_RDONLY);

                    if( tmp.fd > 0 ) {
                        file = wbt_rbtree_insert(&wbt_file_rbtree, file_path);

                        wbt_malloc(&file->value, sizeof(wbt_file_t));
                        wbt_file_t * tmp_file = (wbt_file_t *)file->value.ptr;

                        tmp_file->fd = tmp.fd;
                        tmp_file->refer = 1;
                        tmp_file->size = tmp.size;

                        wbt_log_debug("open file: %d %d", tmp.fd, tmp.size);
                    }
                }
            }
        } else {
            wbt_file_t * tmp_file = (wbt_file_t *)file->value.ptr;
            tmp_file->refer ++;
            
            tmp.fd = tmp_file->fd;
            tmp.size = tmp_file->size;
        }
    }

    return &tmp;
}

wbt_status wbt_file_close( wbt_str_t * file_path ) {
    /* TODO 目前这个函数需要执行一次额外的搜索操作，因为搜索已经在 open 的时候做过了 */
    wbt_rbtree_node_t *file =  wbt_rbtree_get(&wbt_file_rbtree, file_path);

    if(file != NULL) {
        wbt_file_t * tmp_file = (wbt_file_t *)file->value.ptr;

        tmp_file->refer --;

        if( tmp_file->refer == 0 ) {
            /* TODO 需要添加文件句柄的释放机制 */
            /* close(file->fd); */
        }
    }

    return WBT_OK;
}