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

static wbt_fd_t wbt_persist_mid_fd = 0;
static wbt_fd_t wbt_persist_aof_fd = 0;
static ssize_t wbt_persist_aof_size = 0;

static wbt_str_t wbt_persist_mid = wbt_string("./data/webit.mid");
static wbt_str_t wbt_persist_aof = wbt_string("./data/webit.aof");

static wbt_atomic_t wbt_persist_aof_lock = 0;
static wbt_str_t wbt_persist_mdp = wbt_string("./data/webit.mdp");

extern wbt_atomic_t wbt_wating_to_exit;

static wbt_status wbt_mq_persist_timer(wbt_timer_t *timer);
static wbt_status wbt_mq_persist_dump(wbt_timer_t *timer);

void wbt_mq_persist_aof_lock() {
    wbt_persist_aof_lock = 1;
}

void wbt_mq_persist_aof_unlock() {
    wbt_persist_aof_lock = 0;
}

int wbt_mq_persist_aof_is_lock() {
    return wbt_persist_aof_lock;
}

static size_t wbt_mq_persist_recovery_offset = 0;

wbt_status wbt_mq_persist_recovery(wbt_timer_t *timer) {
    wbt_mq_persist_aof_lock();
    
    wbt_msg_block_t *block;
    wbt_msg_t *msg;
    uint32_t crc32;
    ssize_t nread;
    char *data;
    
    block = wbt_malloc(sizeof(wbt_msg_block_t));
    if(!block) {
        goto error;
    }

    int wait_time = 50;
    
    int n = 1000;
    while( !timer || n ) {
        if(wbt_is_oom()) {
            // 内存不足，等待一会儿再尝试恢复
            wait_time = 1000;
            n = 0;
            break;
        }

        nread = wbt_read_file(wbt_persist_aof_fd, block, sizeof(wbt_msg_block_t), wbt_mq_persist_recovery_offset);
        if( nread == 0 ) {
            break;
        } else if( nread != sizeof(wbt_msg_block_t) ) {
            goto error;
        } else {
            wbt_mq_persist_recovery_offset += sizeof(wbt_msg_block_t);
        }

        if( wbt_conf.aof_crc ) {
            if (wbt_read_file(wbt_persist_aof_fd, &crc32, sizeof(uint32_t), wbt_mq_persist_recovery_offset) != sizeof(uint32_t)) {
                goto error;
            } else {
                wbt_mq_persist_recovery_offset += sizeof(uint32_t);
            }
            
            if( crc32 != wbt_crc32( (char *)block, sizeof(wbt_msg_block_t) ) ) {
                // 校验失败
                goto error;
            }
        }
        
        data = wbt_malloc(block->data_len);
        if(!data) {
            goto error;
        }
        if (wbt_read_file(wbt_persist_aof_fd, data, block->data_len, wbt_mq_persist_recovery_offset) != block->data_len) {
            wbt_free( data );
            goto error;
        } else {
            wbt_mq_persist_recovery_offset += block->data_len;
        }

        if( wbt_conf.aof_crc ) {
            if (wbt_read_file(wbt_persist_aof_fd, &crc32, sizeof(uint32_t), wbt_mq_persist_recovery_offset) != sizeof(uint32_t)) {
                wbt_free( data );
                goto error;
            } else {
                wbt_mq_persist_recovery_offset += sizeof(uint32_t);
            }
            
            if( crc32 != wbt_crc32( data, block->data_len ) ) {
                // 校验失败
                wbt_free( data );
                goto error;
            }
        }

        wbt_log_debug( "msg %lld recovered\n", block->msg_id );

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
            wbt_free(data);
        } else if( block->expire > wbt_cur_mtime ) {
            msg = wbt_mq_msg_create(block->msg_id);
            if( msg == NULL ) {
                // * ID 冲突，忽略该条消息，并将该消息从 aof 文件中移除
                // * 注意，只能通过重写 aof 文件来完成移除操作，如果在重写操作完
                // 成之前程序再次重启，aof 文件中将存在一部分 msg_id 冲突的消息
                // * 不使用 fast_boot 可以避免该问题
                wbt_free(data);
            } else {
                wbt_memcpy(msg, block, sizeof(wbt_msg_block_t));
                msg->data = data;

                if( wbt_mq_msg_delivery( msg ) != WBT_OK ) {
                    // TODO 记录该错误
                    wbt_mq_msg_destory( msg );
                }
            }
        } else {
            wbt_free(data);
        }
        
        n --;
    }

//success:
    if(timer && ( n == 0 ) && !wbt_wating_to_exit) {
        /* 重新注册定时事件 */
        timer->timeout += wait_time;

        if(wbt_timer_mod(timer) != WBT_OK) {
            goto error;
        }
    } else {
        wbt_free(timer);
        wbt_mq_persist_aof_unlock();
    }
    
    wbt_free(block);
    
    return WBT_OK;
    
error:

    wbt_free( block );
    wbt_free( timer );

    wbt_mq_persist_aof_unlock();

    return WBT_ERROR;
}

wbt_status wbt_mq_persist_init() {
    // 无论是否启用持久化，mid 都应当被保存，这样可以避免 msg_id 冲突（或减小其发生概率）
    wbt_persist_mid_fd = wbt_open_datafile(wbt_persist_mid.str);
    if ((int)wbt_persist_mid_fd <= 0) {
        return WBT_ERROR;
    }
    
    // 读取 mid
    if (wbt_read_file(wbt_persist_mid_fd, &wbt_mq_persist_count, sizeof(wbt_mq_persist_count), 0) == sizeof(wbt_mq_persist_count)) {
        wbt_mq_msg_update_create_count(wbt_mq_persist_count);
    } else {
        wbt_mq_persist_count = 0;
    }

    if( !wbt_conf.aof ) {
        return WBT_OK;
    }

    wbt_persist_aof_fd = wbt_open_logfile(wbt_persist_aof.str);
    if( (int)wbt_persist_aof_fd <= 0 ) {
        return WBT_ERROR;
    }

    wbt_persist_aof_size = wbt_get_file_size(wbt_persist_aof_fd);
    if( wbt_persist_aof_size < 0 ) {
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

    if( wbt_conf.aof_fast_boot && wbt_mq_persist_count ) {
        wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
        timer->on_timeout = wbt_mq_persist_recovery;
        timer->timeout = wbt_cur_mtime;
        timer->heap_idx = 0;

        if( wbt_timer_add(timer) != WBT_OK ) {
            return WBT_ERROR;
        }

        // TODO 启动后立刻进行重写可能并不合适
        timer = wbt_malloc(sizeof(wbt_timer_t));
        timer->on_timeout = wbt_mq_persist_dump;
        timer->timeout = wbt_cur_mtime + 1; // +1 是为了确保先执行 recovery，再执行 dump
        timer->heap_idx = 0;

        if( wbt_timer_add(timer) != WBT_OK ) {
            return WBT_ERROR;
        }
    } else {
        wbt_mq_persist_recovery(NULL);
        wbt_mq_persist_dump(NULL);
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_persist_check(ssize_t nwrite, size_t len) {
    if( nwrite != len ) {
        if (nwrite == -1) {
            wbt_log_add("Error writing to the AOF file: %d\n", wbt_errno);
        } else {
            wbt_log_add("Short write while writing to "
                            "the AOF file: (nwritten=%lld, "
                            "expected=%lld)\n",
                            (long long)nwrite,
                            (long long)sizeof(wbt_msg_block_t));

            if (wbt_truncate_file(wbt_persist_aof_fd, wbt_persist_aof_size) == -1) {
                wbt_log_add("Could not remove short write "
                                "from the append-only file. The file may be corrupted. "
								"ftruncate: %d\n", wbt_errno);
            }
        }

        return WBT_ERROR;
    }
    
    return WBT_OK;
}

static wbt_status wbt_mq_persist_timer(wbt_timer_t *timer) {
    if( wbt_persist_aof_fd ) {
        // TODO 尝试使用 fdatasync
        if( wbt_sync_file(wbt_persist_aof_fd) != 0 ) {
            // 如果该操作失败，可能是磁盘故障
        }
    }
    
    if( wbt_persist_mid_fd ) {
        ssize_t nwrite = wbt_write_file(wbt_persist_mid_fd, &wbt_mq_persist_count, sizeof(wbt_mq_persist_count), 0);
        if( nwrite < 0 ) {
            // TODO 记录该失败
        } else {
            if (wbt_sync_file_data(wbt_persist_mid_fd) != 0) {
                // 如果该操作失败，可能是磁盘故障
            }
        }
    }
    
    if(timer && !wbt_wating_to_exit) {
        /* 重新注册定时事件 */
        timer->timeout += 1 * 1000;

        if(wbt_timer_mod(timer) != WBT_OK) {
            return WBT_ERROR;
        }
    } else {
        wbt_free(timer);
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_persist_append_to_file(wbt_fd_t fd, wbt_msg_t *msg) {
    ssize_t nwrite = wbt_append_file(fd, msg, sizeof(wbt_msg_block_t));
    if( wbt_mq_persist_check( nwrite, sizeof(wbt_msg_block_t) ) != WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_conf.aof_crc ) {
        uint32_t crc32 = wbt_crc32( (char *)msg, sizeof(wbt_msg_block_t) );

        nwrite = wbt_append_file(fd, &crc32, sizeof(uint32_t));
        if( wbt_mq_persist_check( nwrite, sizeof(crc32) ) != WBT_OK ) {
            return WBT_ERROR;
        }
    }

    nwrite = wbt_append_file(fd, msg->data, msg->data_len);
    if( wbt_mq_persist_check( nwrite, msg->data_len ) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    if( wbt_conf.aof_crc ) {
        uint32_t crc32 = wbt_crc32( (char *)msg->data, msg->data_len );

        nwrite = wbt_append_file(fd, &crc32, sizeof(uint32_t));
        if( wbt_mq_persist_check( nwrite, sizeof(crc32) ) != WBT_OK ) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

// 参数 rf 用于指明是否需要修改数据恢复进度
// 如果该消息已经在内存中生效，则需要
// 如果该消息由于内存不足而延迟生效，则不需要
wbt_status wbt_mq_persist_append(wbt_msg_t *msg, int rf) {
    if( wbt_mq_persist_append_to_file(wbt_persist_aof_fd, msg) != WBT_OK ) {
        return WBT_ERROR;
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
    if( rf ) {
        wbt_mq_persist_recovery_offset += sizeof(wbt_msg_block_t) + msg->data_len;
    }
    
    return WBT_OK;
}

// * 遍历并导出内存中的所有活跃消息
// * 目前不允许对已投递消息做任何修改，所以我们可以将这个操作通过定时事件分步完成。
// * 和 fork 新进程的方式相比，这样做可以节省大量的内存。这允许我们更加频繁地执行该
// 操作。而我们需要付出的唯一代价是导出的文件中可能会存在少量刚刚过期的消息
// TODO 
static wbt_status wbt_mq_persist_dump(wbt_timer_t *timer) {
    // 临时文件 fd
    static wbt_fd_t rdp_fd = 0;
    // 记录当前的最大消息 ID max
    static wbt_mq_id processed_msg_id = 0;

    if( wbt_mq_persist_aof_is_lock() == 1 ) {
        // 如果目前正在从 aof 文件中恢复数据，则 dump 操作必须被推迟
        // 这是为了保证 dump 完成的时刻，所有未过期消息都在内存中
        if(timer && !wbt_wating_to_exit) {
            /* 重新注册定时事件 */
            timer->timeout += 60 * 1000;

            if(wbt_timer_mod(timer) != WBT_OK) {
                return WBT_ERROR;
            }
        }
        
        // 发生数据恢复往往是因为旧的消息被删除了，所以推迟之后，dump 操作应当重新开始
        rdp_fd = 0;
        
        return WBT_OK;
    } else if( 0 /* 如果当前负载过高 */) {
        // 如果当前负载过高，则 dump 操作必须被推迟
        if(timer && !wbt_wating_to_exit) {
            /* 重新注册定时事件 */
            timer->timeout += 3600 * 1000;

            if(wbt_timer_mod(timer) != WBT_OK) {
                return WBT_ERROR;
            }
        }
        
        // 负载过高时，会推迟很长一段时间再尝试，所以推迟之后，dump 操作应当重新开始
        rdp_fd = 0;
        
        return WBT_OK;
    }

    if( !rdp_fd ) {
        // 创建临时文件
        rdp_fd = wbt_open_tmpfile(wbt_persist_mdp.str);
        if( (int)rdp_fd <= 0 ) {
            wbt_log_add("Could not open mdp file: %d\n", wbt_errno);
            return WBT_ERROR;
        }
        processed_msg_id = 0;
    }

    wbt_str_t key;
    wbt_variable_to_str(processed_msg_id, key);
    
    // 从小到大遍历消息，直至 max
    int max = 1000;
    wbt_rb_node_t *node;
    wbt_msg_t *msg;
      
    for (node = wbt_rb_get_greater(wbt_mq_msg_get_all(), &key);
         node;
         node = wbt_rb_next(node)) {
        msg = (wbt_msg_t *)node->value.str;

        if( wbt_mq_persist_append_to_file(rdp_fd, msg) == WBT_OK ) {
            processed_msg_id = msg->msg_id;
        } else {
            return WBT_ERROR;
        }

        if( --max <= 0 && timer ) {
            break;
        }
    }
    
    if( node == NULL ) {
        // 重写完成
        // 如果刷盘策略为 AOF_FSYNC_ALWAYS，我们立刻将数据同步到磁盘上。
        // 如果刷盘策略为 AOF_FSYNC_EVERYSEC，我们什么都不做，只需要等待定时任务执行 fsync(2)。
        // 如果刷盘策略为 AOF_FSYNC_NO，我们什么都不做。
        if( wbt_conf.aof_fsync == AOF_FSYNC_ALWAYS ) {
            wbt_sync_file(rdp_fd);
        }

        // 旧的 aof 文件已经无用了，不需要保证数据落盘，所以直接 close 即可
        if( wbt_close_file(wbt_persist_aof_fd) < 0 ) {
            return WBT_ERROR;
        }
        wbt_persist_aof_fd = 0;

        // 用 mdp 文件覆盖 aof 文件
#ifdef WIN32
        if(wbt_close_file(rdp_fd) < 0) {
            return WBT_ERROR;
        }

        if(MoveFileEx(wbt_persist_mdp.str, wbt_persist_aof.str, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
#else
        if( rename(wbt_persist_mdp.str, wbt_persist_aof.str) < 0 ) {
#endif
            wbt_log_add("Could not rename mdp file. "
                "rename: %d\n", wbt_errno);
            // TODO 需要重新创建 aof 文件
            return WBT_ERROR;
        }

#ifdef WIN32
        wbt_persist_aof_fd = wbt_open_logfile(wbt_persist_aof.str);
        if(wbt_persist_aof_fd <= 0) {
            return WBT_ERROR;
        }
#else
        wbt_persist_aof_fd = rdp_fd;
#endif
        rdp_fd = 0;
        
        // 重写完成时，必然所有消息都在内存中生效，所以直接将 offset 设定为文件末尾
        wbt_mq_persist_recovery_offset = wbt_get_file_size(wbt_persist_aof_fd);

        // 重写完成后，下一次重写将在设定的时间间隔之后运行
        if(!wbt_wating_to_exit) {
            if(!timer) {
                timer = wbt_malloc(sizeof(wbt_timer_t));
                timer->on_timeout = wbt_mq_persist_dump;
                timer->heap_idx = 0;
                timer->timeout = wbt_cur_mtime;
            }

            /* 重新注册定时事件 */
            // TODO 从配置文件中读取
            timer->timeout += 6 * 3600 * 1000;

            if(wbt_timer_mod(timer) != WBT_OK) {
                return WBT_ERROR;
            }
        } else {
            wbt_free(timer);
        }
    } else {
        if(!wbt_wating_to_exit) {
            /* 重新注册定时事件 */
            timer->timeout += 50;

            if(wbt_timer_mod(timer) != WBT_OK) {
                return WBT_ERROR;
            }
        } else {
            wbt_free(timer);
        }
    }

    return WBT_OK;
}