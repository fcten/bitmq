/* 
 * File:   wbt_mq.c
 * Author: fcten
 *
 * Created on 2016年1月14日, 上午10:59
 */

#include "wbt_mq.h"

// 存储所有可用频道
// 一个频道下有一个或多个消费者，如果所有消费者离开频道，该频道将会撤销
extern wbt_rbtree_t wbt_mq_channels;

// 存储所有的订阅者
extern wbt_rbtree_t wbt_mq_subscribers;

// 存储所有已接收到的消息
extern wbt_rbtree_t wbt_mq_messages;

// 所有未生效消息， 根据 effect 排序
extern wbt_heap_t wbt_mq_message_created;

// 所有已生效消息， 根据 expire 排序
extern wbt_heap_t wbt_mq_message_effective;

wbt_status wbt_mq_init() {
    wbt_rbtree_init(&wbt_mq_channels);
    wbt_rbtree_init(&wbt_mq_subscribers);
    wbt_rbtree_init(&wbt_mq_messages);

    if(wbt_heap_init(&wbt_mq_message_created, 1024) != WBT_OK) {
        wbt_log_add("create heap failed\n");
        return WBT_ERROR;
    }

    if(wbt_heap_init(&wbt_mq_message_effective, 1024) != WBT_OK) {
        wbt_log_add("create heap failed\n");
        return WBT_ERROR;
    }

    return WBT_OK;
}

