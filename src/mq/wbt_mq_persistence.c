/* 
 * File:   wbt_mq_persistence.c
 * Author: fcten
 *
 * Created on 2016年2月10日, 下午9:15
 */

#include "wbt_mq_persistence.h"
#include "wbt_mq_msg.h"
#include "../common/wbt_crc.h"

static wbt_mq_id wbt_mq_persist_count = 0;

static int wbt_persist_file_fd = 0;
static ssize_t wbt_persist_file_size = 0;

static wbt_str_t wbt_persist_file = wbt_string("./logs/webit.aof");

extern wbt_atomic_t wbt_wating_to_exit;

wbt_status wbt_mq_persist_timer(wbt_timer_t *timer) {
    if( wbt_persist_file_fd ) {
        // TODO 尝试使用 fdatasync
        fsync(wbt_persist_file_fd);
    }
    
    if(!wbt_wating_to_exit) {
        /* 重新注册定时事件 */
        timer->timeout = wbt_cur_mtime + 1 * 1000;

        if(wbt_timer_mod(timer) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_persist_recovery(wbt_timer_t *timer) {
    wbt_file_t *file = wbt_file_open(&wbt_persist_file);
    // 这里，我们直接把整个 aof 文件读入内存
    // 可以修改为一次只读取一部分
    if( wbt_file_read(file) < 0 ) {
        goto error;
    }
    
    wbt_msg_block_t *block;
    wbt_msg_t *msg;
    uint32_t crc32;
    char *data;

    static char *ptr = 0;
    if(!ptr) {
        ptr = file->ptr;
    }
    
    int n = 1000;
    while(n && file->size - ( ptr - file->ptr ) > 0 ) {
        block = (wbt_msg_block_t *)ptr;
        if( file->size - ( ptr - file->ptr ) < sizeof(wbt_msg_block_t) ) {
            goto error;
        } else {
            ptr += sizeof(wbt_msg_block_t);
        }

        if( wbt_conf.aof_crc ) {
            crc32 = *(uint32_t *)ptr;
            if( file->size - ( ptr - file->ptr ) < sizeof(uint32_t) ) {
                goto error;
            } else {
                ptr += sizeof(uint32_t);
            }
            
            if( crc32 != wbt_crc32( (char *)block, sizeof(wbt_msg_block_t) ) ) {
                // 校验失败
                goto error;
            }
        }
        
        data = ptr;
        if( file->size - ( ptr - file->ptr ) < block->data_len ) {
            goto error;
        } else {
            ptr += block->data_len;
        }

        if( wbt_conf.aof_crc ) {
            crc32 = *(uint32_t *)(ptr);
            if( file->size - ( ptr - file->ptr ) < sizeof(uint32_t) ) {
                goto error;
            } else {
                ptr += sizeof(uint32_t);
            }
            
            if( crc32 != wbt_crc32( data, block->data_len ) ) {
                // 校验失败
                goto error;
            }
        }

        // 由于内存和文件中保存的结构并不一致，这里我们不得不进行内存拷贝
        // TODO 修改消息相关的内存操作，尝试直接使用读入的内存
        // TODO 使用 mdb 文件加速载入
        if( block->expire > wbt_cur_mtime ) {
            msg = wbt_mq_msg_create(block->msg_id);
            if( msg == NULL ) {
                goto error;
            }
            wbt_memcpy(msg, block, sizeof(wbt_msg_block_t));
            msg->data = wbt_strdup(data, block->data_len);
            
            if( wbt_mq_msg_delivery( msg ) != WBT_OK ) {
                wbt_mq_msg_destory( msg );
                
                // wbt_log_add
            }
        }
        
        n --;
    }
    
    wbt_file_close(file);
    
    if(!wbt_wating_to_exit && file->size - ( ptr - file->ptr ) > 0 ) {
        /* 重新注册定时事件 */
        timer->timeout = wbt_cur_mtime + 50;

        if(wbt_timer_mod(timer) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
    
error:
    
    wbt_file_close(file);
    return WBT_ERROR;
}

wbt_status wbt_mq_persist_init() {
    wbt_persist_file_fd = open(wbt_persist_file.str,
            O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0777);
    if( wbt_persist_file_fd <= 0 ) {
        return WBT_ERROR;
    }

    wbt_persist_file_size = wbt_file_size(&wbt_persist_file);
    if( wbt_persist_file_size < 0 ) {
        return WBT_ERROR;
    }

    // redis 2.4. 以后开始使用后台线程异步刷盘，我们暂时不那么做
    if( wbt_conf.aof_fsync == AOF_FSYNC_EVERYSEC ) {
        wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
        timer->on_timeout = wbt_mq_persist_timer;
        timer->timeout = wbt_cur_mtime + 1 * 1000;
        timer->heap_idx = 0;

        if( wbt_timer_add(timer) != WBT_OK ) {
            return WBT_ERROR;
        }
    }

    wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
    timer->on_timeout = wbt_mq_persist_recovery;
    timer->timeout = wbt_cur_mtime;
    timer->heap_idx = 0;

    if( wbt_timer_add(timer) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_persist_check(ssize_t nwrite, size_t len) {
    if( nwrite != len ) {
        if (nwrite == -1) {
            wbt_log_add("Error writing to the AOF file: %s",
                strerror(errno));
        } else {
            wbt_log_add("Short write while writing to "
                            "the AOF file: (nwritten=%lld, "
                            "expected=%lld)",
                            (long long)nwrite,
                            (long long)sizeof(wbt_msg_block_t));

            if (ftruncate(wbt_persist_file_fd, wbt_persist_file_size) == -1) {
                wbt_log_add("Could not remove short write "
                                "from the append-only file. The file may be corrupted. "
                                "ftruncate: %s", strerror(errno));
            }
        }

        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_persist(wbt_msg_t *msg) {
    return WBT_OK;
    ssize_t nwrite = write(wbt_persist_file_fd, msg, sizeof(wbt_msg_block_t));
    if( wbt_mq_persist_check( nwrite, sizeof(wbt_msg_block_t) ) != WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_conf.aof_crc ) {
        uint32_t crc32 = wbt_crc32( (char *)msg, sizeof(wbt_msg_block_t) );

        nwrite = write(wbt_persist_file_fd, &crc32, sizeof(uint32_t));
        if( wbt_mq_persist_check( nwrite, sizeof(crc32) ) != WBT_OK ) {
            return WBT_ERROR;
        }
    }

    nwrite = write(wbt_persist_file_fd, msg->data, msg->data_len);
    if( wbt_mq_persist_check( nwrite, msg->data_len ) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    if( wbt_conf.aof_crc ) {
        uint32_t crc32 = wbt_crc32( (char *)msg->data, msg->data_len );

        nwrite = write(wbt_persist_file_fd, &crc32, sizeof(uint32_t));
        if( wbt_mq_persist_check( nwrite, sizeof(crc32) ) != WBT_OK ) {
            return WBT_ERROR;
        }
    }
    
    if( wbt_conf.aof_fsync == AOF_FSYNC_ALWAYS ) {
        if( fsync(wbt_persist_file_fd) != 0 ) {
            // 如果该操作失败，可能是磁盘故障
        }
    }
    
    wbt_persist_file_size += sizeof(wbt_msg_block_t) + msg->data_len;
    
    return WBT_OK;
}
