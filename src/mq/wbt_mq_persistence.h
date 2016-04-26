/* 
 * File:   wbt_mq_persistence.h
 * Author: fcten
 *
 * Created on 2016年2月10日, 下午9:15
 */

#ifndef WBT_MQ_PERSISTENCE_H
#define	WBT_MQ_PERSISTENCE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

// 这个结构仅仅用于计算 wbt_msg_t 结构中所需要持久化部分的长度
// 总是应当保持和 wbt_msg_t 同步更新
typedef struct wbt_msg_block_s {
    // 消息 ID
    wbt_mq_id msg_id;
    // 消息生产者所订阅的频道 ID
    wbt_mq_id producer_id;
    // 消息消费者所订阅的频道 ID
    wbt_mq_id consumer_id;
    // 创建时间
    time_t create;
    // 生效时间
    time_t effect;
    // 过期时间
    time_t expire;
    // 优先级
    // 数值较小的消息会被优先处理
    //unsigned int priority:4;
    // 投递模式
    // 1、负载均衡模式：有多个消费者。消息以负载均衡的方式投递给某一个消费者。
    // 2、广播模式：有多个消费者。消息以广播的方式投递给所有消费者。
    unsigned int type:2;
    // 投递质量保证
    unsigned int qos:2;
    // 消息长度
    size_t data_len;
} wbt_msg_block_t;
    
wbt_status wbt_mq_persist_init();

wbt_status wbt_mq_persist_append(wbt_msg_t *msg, int rf);

wbt_status wbt_mq_persist_recovery(wbt_timer_t *timer);

int wbt_mq_persist_aof_lock();

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_PERSISTENCE_H */

