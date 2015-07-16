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
#include "wbt_event.h"
#include "wbt_time.h"

wbt_module_t wbt_module_file = {
    wbt_string("file"),
    wbt_file_init
};

wbt_file_t tmp;

#define WBT_MAX_OPEN_FILES 1024

wbt_rbtree_t wbt_file_rbtree;

extern wbt_rbtree_node_t *wbt_rbtree_node_nil;

int wbt_lock_accept;

void wbt_file_cleanup_recursive(wbt_rbtree_node_t *node) {
    /* 从叶节点递归处理至根节点 */
    if(node != wbt_rbtree_node_nil) {
        wbt_file_cleanup_recursive(node->left);
        wbt_file_cleanup_recursive(node->right);
        
        wbt_file_t * tmp_file = (wbt_file_t *)node->value.ptr;
        if( tmp_file->refer == 0 && cur_mtime - tmp_file->last_use_mtime > 10000 ) {
            wbt_log_debug("closed fd:%d %.*s", tmp_file->fd, node->key.len, node->key.ptr);
            close(tmp_file->fd);
            wbt_rbtree_delete(&wbt_file_rbtree, node);
        }
    }
}

wbt_status wbt_file_cleanup(wbt_event_t *ev) {
    wbt_log_debug("opened fd before cleanup: %d", wbt_file_rbtree.size);
    
    wbt_file_cleanup_recursive(wbt_file_rbtree.root);
    
    wbt_log_debug("opened fd after cleanup: %d", wbt_file_rbtree.size);

    /* 重新注册定时事件 */
    ev->time_out = cur_mtime + 10000;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_file_init() {
    /* 初始化一个红黑树用以保存已打开的文件句柄 */
    wbt_rbtree_init(&wbt_file_rbtree);

    /* 添加一个定时任务用以清理过期的文件句柄 */
    wbt_event_t tmp_ev;
    tmp_ev.callback = wbt_file_cleanup;
    tmp_ev.fd = -1;
    /* tmp_ev.events = 0; // fd 大于等于 0 时该属性才有意义 */
    tmp_ev.time_out = cur_mtime + 10000;

    if(wbt_event_add(&tmp_ev) == NULL) {
        return WBT_ERROR;
    }
    
    if( ( wbt_lock_accept = wbt_lock_create("/tmp/.wbt_accept_lock") ) <= 0 ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_file_t * wbt_file_open( wbt_str_t * file_path ) {    
    wbt_log_debug("try to open: %.*s", file_path->len, file_path->str);
    /* TODO 不存在的文件也可以考虑放入文件树内
     * 如果发生大量的 404 请求，可以提高性能
     */
    
    tmp.offset = 0;

    if( file_path->len >= 512 ) {
        /* 路径过长则拒绝打开 */
        /* 由于接收数据长度存在限制，这里无需担心溢出 */
        wbt_log_debug("file path too long");

        tmp.fd = -3;
    } else {
        wbt_rbtree_node_t *file =  wbt_rbtree_get(&wbt_file_rbtree, file_path);
        if( file == NULL ) {
            struct stat statbuff;  
            if(stat(file_path->str, &statbuff) < 0){  
                /* 文件访问失败 */
                tmp.size = 0;
                if( errno == ENOENT ) {
                    tmp.fd   = -1;  /* 文件不存在 */
                } else {
                    tmp.fd   = -2;  /* 权限不足或者其他错误 */
                }
                wbt_log_debug("stat: %s", strerror(errno));
            }else{  
                if( S_ISDIR(statbuff.st_mode) ) {
                     /* 尝试打开的是目录 */
                    tmp.size = 0;
                    tmp.fd   = -2;
                } else {
                    /* 尝试打开的是文件 */
                    tmp.size = statbuff.st_size;
                    tmp.fd = open(file_path->str, O_RDONLY);
                    tmp.last_modified =  statbuff.st_mtime;

                    if( tmp.fd > 0 ) {
                        file = wbt_rbtree_insert(&wbt_file_rbtree, file_path);

                        wbt_malloc(&file->value, sizeof(wbt_file_t));
                        wbt_file_t * tmp_file = (wbt_file_t *)file->value.ptr;

                        tmp_file->fd = tmp.fd;
                        tmp_file->refer = 1;
                        tmp_file->size = tmp.size;
                        tmp_file->last_modified = tmp.last_modified;

                        wbt_log_debug("open file: %d %d", tmp.fd, tmp.size);
                    } else {
                        tmp.size = 0;
                        if( errno == EACCES ) {
                            tmp.fd   = -2;
                        } else {
                            tmp.fd   = -1;
                        }
                    }
                }
            }
        } else {
            wbt_file_t * tmp_file = (wbt_file_t *)file->value.ptr;
            tmp_file->refer ++;
            
            tmp.fd = tmp_file->fd;
            tmp.size = tmp_file->size;
            tmp.last_modified = tmp_file->last_modified;
        }
    }

    return &tmp;
}

wbt_status wbt_file_close( wbt_str_t * file_path ) {
    wbt_log_debug("try to close: %.*s", file_path->len, file_path->str);
    /* TODO 目前这个函数需要执行一次额外的搜索操作，因为搜索已经在 open 的时候做过了 */
    wbt_rbtree_node_t *file =  wbt_rbtree_get(&wbt_file_rbtree, file_path);

    if(file != NULL) {
        wbt_file_t * tmp_file = (wbt_file_t *)file->value.ptr;

        tmp_file->refer --;

        if( tmp_file->refer == 0 ) {
            /* 当所有对该文件的使用都被释放后，更新时间戳 */
            tmp_file->last_use_mtime = cur_mtime;
        }
    }

    return WBT_OK;
}

wbt_status wbt_trylock_fd(int fd) {
    struct flock  fl;

    memset(&fl, 0, sizeof(struct flock));
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_lock_fd(int fd) {
    struct flock  fl;

    memset(&fl, 0, sizeof(struct flock));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_unlock_fd(int fd) {
    struct flock  fl;

    memset(&fl, 0, sizeof(struct flock));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return  WBT_ERROR;
    }

    return WBT_OK;
}

int wbt_lock_create( const char *name ) {
    return open( name, O_RDWR | O_CREAT, S_IRWXU );
}
