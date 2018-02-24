/* 
 * File:   wbt_mq_snowflake.h
 * Author: fcten
 *
 * Created on 2018年2月23日, 上午11:51
 * 
 * 这是一个 snowflake 分布式 ID 生成算法的代码实现。
 * 
 * - 保证 ID 趋势递增（但不是严格递增，取决于机器之间的时间差）
 * - 允许通过借用未来的时间戳来应对时钟回拨以及流量洪峰，代价是损失趋势递增保证
 * 
 * TODO cluster 模式下自动分配 worker_id
 */

#ifndef WBT_MQ_SNOWFLAKE_H
#define WBT_MQ_SNOWFLAKE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

// 时间戳起始时间 2018-1-1 0:0:0
#define SF_EPOCH          1514736000000UL
// 最大序列号
#define SF_MAX_SEQ_ID     ((1 << 12) - 1)
// 最大 worker id
#define SF_MAX_WORKER_ID  ((1 << 10) - 1)
    // * 由于在单次 epoll 循环中 wbt_cur_mtime 不会被更新，所以必须允许 ID 分配器借用
// 未来的时间用于分配 ID。不建议将该值设定得太小。
// * 如果开启了 ntp 时间同步，可能会导致服务器时间回退。合理设定该值可以避免服务出现异
// 常。
// * 如果服务器时间回退过长，超出了该值所设定的范围，BitMQ 会拒绝所有新消息，直到服务器
// 时间恢复正常。
#define SF_MAX_ERR_RANGE  100

typedef struct wbt_sf_id_s {
    uint64_t timestamp:42;
    uint64_t worker_id:10;
    uint64_t seq_id:12;
} wbt_sf_t;

void wbt_mq_init_id(wbt_sf_t *sf, int worker_id);
void wbt_mq_set_id(wbt_sf_t *sf, wbt_mq_id msg_id);
void wbt_mq_update_id(wbt_sf_t *sf, wbt_mq_id msg_id);
wbt_mq_id wbt_mq_next_id(wbt_sf_t *sf);

static wbt_inline wbt_mq_id wbt_mq_cur_id(wbt_sf_t *sf) {
    return ( (wbt_mq_id)sf->timestamp << 22 ) | ( (wbt_mq_id)sf->worker_id << 12 ) | (wbt_mq_id)sf->seq_id;
}

#ifdef __cplusplus
}
#endif

#endif /* WBT_MQ_SNOWFLAKE_H */

