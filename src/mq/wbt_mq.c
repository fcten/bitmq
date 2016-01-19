/* 
 * File:   wbt_mq.c
 * Author: fcten
 *
 * Created on 2016年1月14日, 上午10:59
 */

#include "wbt_mq.h"
#include "../common/wbt_module.h"

wbt_module_t wbt_module_mq = {
    wbt_string("mq"),
    wbt_mq_init, // init
    NULL, // exit
    NULL, // on_conn
    NULL, // on_recv
    NULL, // on_send
    NULL  // on_close
};

// 存储所有可用频道
// 一个频道下有一个或多个消费者，如果所有消费者离开频道，该频道将会撤销
extern wbt_rbtree_t wbt_mq_channels;

// 存储所有的订阅者
extern wbt_rbtree_t wbt_mq_subscribers;

// 存储所有已接收到的消息
extern wbt_rbtree_t wbt_mq_messages;

// 存储所有未生效的消息
extern wbt_heap_t wbt_mq_message_delayed;

wbt_status wbt_mq_init() {
    wbt_rbtree_init(&wbt_mq_channels);
    wbt_rbtree_init(&wbt_mq_subscribers);
    wbt_rbtree_init(&wbt_mq_messages);

    if(wbt_heap_init(&wbt_mq_message_delayed, 1024) != WBT_OK) {
        wbt_log_add("create heap failed\n");
        return WBT_ERROR;
    }

    return WBT_OK;
}

