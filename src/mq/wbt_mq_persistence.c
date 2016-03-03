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

static int wbt_persist_mid_fd = 0;
static int wbt_persist_aof_fd = 0;
static ssize_t wbt_persist_aof_size = 0;

static wbt_str_t wbt_persist_mid = wbt_string("./data/webit.mid");
static wbt_str_t wbt_persist_aof = wbt_string("./data/webit.aof");

extern wbt_atomic_t wbt_wating_to_exit;

static wbt_status wbt_mq_persist_timer(wbt_timer_t *timer);

wbt_status wbt_mq_persist_recovery(wbt_timer_t *timer) {
    wbt_file_t *file = wbt_file_open(&wbt_persist_aof);
    // 这里，我们直接把整个 aof 文件读入内存
    // 可以修改为一次只读取一部分
    if( wbt_file_read(file) <= 0 ) {
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
    while((!timer || n) && file->size - ( ptr - file->ptr ) > 0 ) {
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
        // 已过期的消息应当被忽略
        if( block->type == MSG_ACK ) {
            // 未过期但是已经收到 ACK 的负载均衡消息可能会在 fast_boot 期间被重复处理
            wbt_msg_t *msg = wbt_mq_msg_get(block->consumer_id);
            if(msg) {
                wbt_mq_msg_destory(msg);
            }
        } else if( block->expire > wbt_cur_mtime ) {
            msg = wbt_mq_msg_create(block->msg_id);
            if( msg == NULL ) {
                // * 这里发生失败，可能是因为内存不足，也可能是因为 msg_id 冲突
                // * 如果是因为内存不足，则应该等待一会儿再尝试恢复
                // * 如果是因为 ID 冲突，则应该忽略该条消息，并将该消息从 aof 文
                // 件中移除
                // * 注意，只能通过重写 aof 文件来完成移除操作，如果在重写操作完
                // 成之前程序再次重启，aof 文件中将存在一部分 msg_id 冲突的消息
                // * 不使用 fast_boot 可以避免该问题
                goto error;
            }
            wbt_memcpy(msg, block, sizeof(wbt_msg_block_t));
            msg->data = wbt_strdup(data, block->data_len);
            
            if( wbt_mq_msg_delivery( msg ) != WBT_OK ) {
                // TODO 记录该错误
                wbt_mq_msg_destory( msg );
            }
        }
        
        n --;
    }
    
    wbt_file_close(file);
    
    if(timer && file->size - ( ptr - file->ptr ) > 0 && !wbt_wating_to_exit) {
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
    wbt_file_t *file = wbt_file_open(&wbt_persist_mid);
    if( wbt_file_read(file) > 0 ) {
        wbt_mq_persist_count = *(wbt_mq_id*)file->ptr;
        wbt_mq_msg_update_create_count(wbt_mq_persist_count);
    }
    wbt_file_close(file);

    // 无论是否启用持久化，mid 都应当被保存，这样可以避免 msg_id 冲突（或减小其发生概率）
    wbt_persist_mid_fd = open(wbt_persist_mid.str,
            O_WRONLY | O_CREAT | O_CLOEXEC, 0777);
    if( wbt_persist_mid_fd <= 0 ) {
        return WBT_ERROR;
    }

    if( wbt_conf.aof ) {
        wbt_persist_aof_fd = open(wbt_persist_aof.str,
                O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0777);
        if( wbt_persist_aof_fd <= 0 ) {
            return WBT_ERROR;
        }

        wbt_persist_aof_size = wbt_file_size(&wbt_persist_aof);
        if( wbt_persist_aof_size < 0 ) {
            return WBT_ERROR;
        }
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

    if( wbt_conf.aof_fast_boot && wbt_mq_persist_count ) {
        wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
        timer->on_timeout = wbt_mq_persist_recovery;
        timer->timeout = wbt_cur_mtime + 1;
        timer->heap_idx = 0;

        if( wbt_timer_add(timer) != WBT_OK ) {
            return WBT_ERROR;
        }
    } else {
        wbt_mq_persist_recovery(NULL);
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

            if (ftruncate(wbt_persist_aof_fd, wbt_persist_aof_size) == -1) {
                wbt_log_add("Could not remove short write "
                                "from the append-only file. The file may be corrupted. "
                                "ftruncate: %s", strerror(errno));
            }
        }

        return WBT_ERROR;
    }
    
    return WBT_OK;
}

static wbt_status wbt_mq_persist_timer(wbt_timer_t *timer) {
    if( wbt_persist_aof_fd ) {
        // TODO 尝试使用 fdatasync
        if( fsync(wbt_persist_aof_fd) != 0 ) {
            // 如果该操作失败，可能是磁盘故障
        }
    }
    
    if( wbt_persist_mid_fd ) {
        ssize_t nwrite = pwrite(wbt_persist_mid_fd, &wbt_mq_persist_count, sizeof(wbt_mq_persist_count), 0);
        if( nwrite < 0 ) {
            // TODO 记录该失败
        } else {
            if( fdatasync(wbt_persist_mid_fd) != 0 ) {
                // 如果该操作失败，可能是磁盘故障
            }
        }
    }
    
    if(timer && !wbt_wating_to_exit) {
        /* 重新注册定时事件 */
        timer->timeout = wbt_cur_mtime + 1 * 1000;

        if(wbt_timer_mod(timer) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_persist(wbt_msg_t *msg) {
    ssize_t nwrite = write(wbt_persist_aof_fd, msg, sizeof(wbt_msg_block_t));
    if( wbt_mq_persist_check( nwrite, sizeof(wbt_msg_block_t) ) != WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_conf.aof_crc ) {
        uint32_t crc32 = wbt_crc32( (char *)msg, sizeof(wbt_msg_block_t) );

        nwrite = write(wbt_persist_aof_fd, &crc32, sizeof(uint32_t));
        if( wbt_mq_persist_check( nwrite, sizeof(crc32) ) != WBT_OK ) {
            return WBT_ERROR;
        }
    }

    nwrite = write(wbt_persist_aof_fd, msg->data, msg->data_len);
    if( wbt_mq_persist_check( nwrite, msg->data_len ) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    if( wbt_conf.aof_crc ) {
        uint32_t crc32 = wbt_crc32( (char *)msg->data, msg->data_len );

        nwrite = write(wbt_persist_aof_fd, &crc32, sizeof(uint32_t));
        if( wbt_mq_persist_check( nwrite, sizeof(crc32) ) != WBT_OK ) {
            return WBT_ERROR;
        }
    }
    
    // 记录持久化进度
    // * 注意，在实际使用过程中，由于该值的写入顺序在消息写入之后，所以
    // 该值可能落后于真正的持久化进度，msg_id 大于该值的消息可能会因为
    // msg_id 冲突而无法恢复
    // * 不使用 fast_boot 可以避免该问题
    // * 如果未能获取该值，则无法使用 fast_boot
    wbt_mq_persist_count = msg->msg_id;
    
    if( wbt_conf.aof_fsync == AOF_FSYNC_ALWAYS ) {
        wbt_mq_persist_timer(NULL);
    }
    
    wbt_persist_aof_size += sizeof(wbt_msg_block_t) + msg->data_len;
    
    return WBT_OK;
}

wbt_status wbt_mq_persist_rewrite() {
    // * 重写 aof 其实是遍历并导出内存中的所有活跃消息
    // * 目前不允许对已投递消息做任何修改，所以我们可以将这个操作通过定时事件分步完成。
    // * 和 fork 新进程的方式相比，这样做可以节省大量的内存。这允许我们更加频繁地执行该
    // 操作。而我们需要付出的唯一代价是重写后的文件中依然会存在少量刚刚过期的消息

    // 创建并使用新的 aof 文件
    
    // 记录当前的最大消息 ID max
    
    // 在定时任务中，从小到大遍历消息，直至 max
    
    // 重写完成，删除旧的 aof 文件
    // 如果在程序启动时发现了旧的 aof 文件，必须做相应处理
    
    return WBT_OK;
}