/* 
 * File:   wbt_mq.c
 * Author: fcten
 *
 * Created on 2016年1月14日, 上午10:59
 */

#include "wbt_mq.h"

// 存储所有可用频道
// 一个频道下有一个或多个消费者，如果所有消费者离开频道，该频道将会撤销
wbt_rbtree_t wbt_mq_channels;

// 存储所有的订阅者
wbt_subscriber_t wbt_mq_subscribers;

// 存储所有已接收到的消息
wbt_msg_t wbt_mq_messages;

// 所有未生效消息， 根据 effect 排序
wbt_heap_t wbt_mq_message_created;

// 所有已生效消息， 根据 expire 排序
wbt_heap_t wbt_mq_message_effective;

wbt_status wbt_mq_init() {
    wbt_rbtree_init(&wbt_mq_channels);
    wbt_list_init(&wbt_mq_subscribers.list);
    wbt_list_init(&wbt_mq_messages.list);
    
    return WBT_OK;
}

wbt_msg_t * wbt_mq_message_create() {
    wbt_mem_t tmp;
    if( wbt_malloc(&tmp, sizeof(wbt_msg_t)) != WBT_OK ) {
        return NULL;
    }
    wbt_memset(&tmp, 0);
    
    static wbt_mq_id auto_inc_id = 0;
    wbt_msg_t * msg = tmp.ptr;
    
    msg->msg_id = ++auto_inc_id;
    msg->create = wbt_cur_mtime;
    
    return msg;
}

void wbt_mq_message_destory(wbt_msg_t *msg) {
    wbt_mem_t tmp;
    tmp.ptr = msg;
    tmp.len = sizeof(wbt_msg_t);
    wbt_free(&tmp);
}

wbt_status wbt_mq_message_add(wbt_msg_t *msg) {
    wbt_list_add(&msg->list, &wbt_mq_messages.list);

    wbt_heap_node_t msg_node;
    msg_node.ptr = msg;
    //  msg_node.modified = msg->modified;
    
    if( msg->effect > wbt_cur_mtime ) {
        // 加入未生效队列
        msg_node.timeout = msg->effect;
        if(wbt_heap_insert(&wbt_mq_message_created, &msg_node) != WBT_OK) {
            return WBT_ERROR;
        }
    } else if( msg->expire > wbt_cur_mtime ) {
        // 加入已生效队列
        msg_node.timeout = msg->expire;
        if(wbt_heap_insert(&wbt_mq_message_effective, &msg_node) != WBT_OK) {
            return WBT_ERROR;
        }
    } else {
        // 不应当尝试添加已过期消息
        return WBT_ERROR;
    }
    
    
    
    return WBT_OK;
}

