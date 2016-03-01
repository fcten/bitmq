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
#include "wbt_gzip.h"

wbt_module_t wbt_module_file = {
    wbt_string("file"),
    wbt_file_init,
    wbt_file_exit
};

wbt_file_t tmp;

#define WBT_MAX_OPEN_FILES 1024

wbt_rbtree_t wbt_file_rbtree;

extern wbt_atomic_t wbt_wating_to_exit;

void wbt_file_cleanup_recursive(wbt_rbtree_node_t *node) {
    /* 从叶节点递归处理至根节点 */
    if(node) {
        wbt_file_cleanup_recursive(node->left);
        wbt_file_cleanup_recursive(node->right);
        
        wbt_file_t * tmp_file = (wbt_file_t *)node->value.str;
        if( tmp_file->refer == 0 && wbt_cur_mtime - tmp_file->last_use_mtime > 10000 ) {
            wbt_log_debug("closed fd:%d %.*s\n", tmp_file->fd, node->key.len, node->key.str.s);
            close(tmp_file->fd);
            wbt_free(tmp_file->ptr);
            wbt_free(tmp_file->gzip_ptr);
            wbt_rbtree_delete(&wbt_file_rbtree, node);
        }
    }
}

wbt_status wbt_file_cleanup(wbt_timer_t *timer) {
    //wbt_log_debug("opened fd before cleanup: %d\n", wbt_file_rbtree.size);
    
    // TODO 遍历所有文件可能会花费过多的时间
    wbt_file_cleanup_recursive(wbt_file_rbtree.root);
    wbt_mem_print();
    
    //wbt_log_debug("opened fd after cleanup: %d\n", wbt_file_rbtree.size);

    if(!wbt_wating_to_exit) {
        /* 重新注册定时事件 */
        timer->timeout = wbt_cur_mtime + 10000;

        if(wbt_timer_mod(timer) != WBT_OK) {
            return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_file_exit() {
    return WBT_OK;
}

wbt_status wbt_file_init() {
    /* 初始化一个红黑树用以保存已打开的文件句柄 */
    wbt_rbtree_init(&wbt_file_rbtree);

    /* 添加一个定时任务用以清理过期的文件句柄 */
    wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
    timer->on_timeout  = wbt_file_cleanup;
    timer->timeout     = wbt_cur_mtime + 10000;
    timer->heap_idx    = 0;

    if(wbt_timer_add(timer) != WBT_OK) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_file_t * wbt_file_open( wbt_str_t * file_path ) {    
    //wbt_log_debug("try to open: %.*s\n", file_path->len, file_path->str);
    /* TODO 不存在的文件也可以考虑放入文件树内
     * 如果发生大量的 404 请求，可以提高性能
     */

    if( file_path->len >= 512 ) {
        /* 路径过长则拒绝打开 */
        /* 由于接收数据长度存在限制，这里无需担心溢出 */
        wbt_log_debug("file path too long\n");

        tmp.fd = -3;
        
        return &tmp;
    }

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
            //wbt_log_debug("stat: %s\n", strerror(errno));
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

                    file->value.str = wbt_calloc(sizeof(wbt_file_t));
                    if( file->value.str == NULL ) {
                        // TODO 如果这里失败了，该文件将永远不会被关闭
                        wbt_rbtree_delete(&wbt_file_rbtree, file);
                    } else {
                        wbt_file_t * tmp_file = (wbt_file_t *)file->value.str;

                        tmp_file->fd = tmp.fd;
                        tmp_file->refer = 1;
                        tmp_file->size = tmp.size;
                        tmp_file->last_modified = tmp.last_modified;

                        wbt_log_debug("open file: %d %zd\n", tmp_file->fd, tmp_file->size);
                        return tmp_file;
                    }
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
        wbt_file_t * tmp_file = (wbt_file_t *)file->value.str;
        tmp_file->refer ++;

        return tmp_file;
    }

    return &tmp;
}

wbt_status wbt_file_close( wbt_file_t * file ) {
    //wbt_log_debug("try to close: %.*s\n", file_path->len, file_path->str);
    if(file != NULL) {

        file->refer --;

        if( file->refer == 0 ) {
            /* 当所有对该文件的使用都被释放后，更新时间戳 */
            file->last_use_mtime = wbt_cur_mtime;
        }
        
        wbt_log_debug("close file: %d %d\n", file->fd, file->refer);
    }

    return WBT_OK;
}

/* 尝试对指定文件加锁
 * 如果加锁失败，进程将会睡眠，直到加锁成功
 */
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

/* 尝试对指定文件加锁
 * 如果加锁失败，立刻出错返回
 */
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
    return open( name, O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU );
}

ssize_t wbt_file_size(wbt_str_t * file_path) {
    struct stat statbuff;  
    if(stat(file_path->str, &statbuff) < 0){  
        return -1;
    }else{  
        if( S_ISDIR(statbuff.st_mode) ) {
            return -1;
        } else {
            return statbuff.st_size;
        }
    }
}

ssize_t wbt_file_read( wbt_file_t *file ) {
    if( !file->fd ) {
        return -1;
    }
    
    if( !file->ptr ) {
        file->ptr = wbt_malloc(file->size);
        if( !file->ptr ) {
            return -1;
        }
        return pread(file->fd, file->ptr, file->size, 0);
    }
    
    return 0;
}

wbt_status wbt_file_compress( wbt_file_t *file ) {
    if( !file->ptr && wbt_file_read( file ) < 0 ) {
        return WBT_ERROR;
    }
    
    if( file->gzip_ptr ) {
        return WBT_OK;
    }
    
    int ret;
    size_t size = file->size > 1024 ? file->size : 1024;
    do {
        // 最大分配 16M
        if( size > 1024 * 1024 * 16 ) {
            wbt_free( file->gzip_ptr );
            file->gzip_ptr = NULL;
            return WBT_ERROR;
        }

        void *p = wbt_realloc( file->gzip_ptr, size );
        if( p == NULL ) {
            wbt_free( file->gzip_ptr );
            file->gzip_ptr = NULL;
            return WBT_ERROR;
        }
        file->gzip_ptr = p;
        file->gzip_size = size;

        ret = wbt_gzip_compress((Bytef *)file->ptr,
                (uLong)file->size,
                (Bytef *)file->gzip_ptr,
                (uLong *)&file->gzip_size );

        if( ret == Z_OK ) {
            file->gzip_ptr = wbt_realloc( file->gzip_ptr, file->gzip_size );
            return WBT_OK;
        } else if( ret == Z_BUF_ERROR ) {
            // 缓冲区不够大
            size *= 2;
        } else {
            // 内存不足
            wbt_free( file->gzip_ptr );
            file->gzip_ptr = NULL;
            return WBT_ERROR;
        }
    } while( ret == Z_BUF_ERROR );

    return WBT_ERROR;
}