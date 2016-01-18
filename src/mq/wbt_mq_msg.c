/* 
 * File:   wbt_mq_msg.c
 * Author: fcten
 *
 * Created on 2016年1月18日, 下午4:08
 */

#include "wbt_mq_msg.h"

// 存储所有可用频道
// 一个频道下有一个或多个消费者，如果所有消费者离开频道，该频道将会撤销
extern wbt_rbtree_t wbt_mq_channels;

// 存储所有已接收到的消息
wbt_rbtree_t wbt_mq_messages;

// 所有未生效消息， 根据 effect 排序
wbt_heap_t wbt_mq_message_created;

// 所有已生效消息， 根据 expire 排序
wbt_heap_t wbt_mq_message_effective;

wbt_msg_t * wbt_mq_msg_create() {
    static wbt_mq_id auto_inc_id = 0;
    wbt_msg_t * msg = wbt_new(wbt_msg_t);
    
    if( msg ) {
        msg->msg_id = ++auto_inc_id;
        msg->create = wbt_cur_mtime;
    }
    
    return msg;
}

void wbt_mq_msg_destory(wbt_msg_t *msg) {
    wbt_delete(msg);
}

wbt_status wbt_mq_msg_delivery(wbt_msg_t *msg) {
    wbt_heap_node_t msg_node;
    msg_node.ptr = msg;
    //  msg_node.modified = msg->modified;
    
    if( msg->effect > wbt_cur_mtime ) {
        // 加入未生效队列
        msg_node.timeout = msg->effect;
        return wbt_heap_insert(&wbt_mq_message_created, &msg_node);
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
    
    wbt_str_t channel_id;
    channel_id.str = (u_char *)&msg->consumer_id;
    channel_id.len = sizeof(msg->consumer_id);
    wbt_channel_t * channel = wbt_rbtree_get_value(&wbt_mq_channels, &channel_id);
    if( channel == NULL ) {
        // 投递的频道不存在，投递失败
        // TODO 新建频道？
        return WBT_ERROR;
    }
    
    if( msg->delivery_mode == MSG_BROADCAST ) {
        // 广播模式，消息将投递给该频道下所有的订阅者
         wbt_subscriber_list_t * subscriber_list;
        wbt_subscriber_t * subscriber;
        wbt_mem_t tmp_mem;
        wbt_list_for_each_entry( subscriber_list, &channel->subscriber_list->list, list ) {
            subscriber = subscriber_list->subscriber;
            if( wbt_malloc( &tmp_mem, sizeof(wbt_msg_list_t) ) == WBT_OK ) {
                if( wbt_list_empty(&subscriber->msg_list->list) &&
                    subscriber->ev->on_timeout == NULL/* TODO */ ) {
                    // TODO 添加 EPOLL_OUT 监听
                }

                wbt_msg_list_t * tmp_node = tmp_mem.ptr;
                tmp_node->msg = msg;
                wbt_list_add_tail( &tmp_node->list, &subscriber->msg_list->list );
            } else {
                // 内存不足，投递失败
                // TODO 需要记录该次投递失败
                continue;
            }
        }
    } else if( msg->delivery_mode == MSG_LOAD_BALANCE ) {
        // 负载均衡模式，消息将投递给可用队列中的第一个订阅者
        wbt_subscriber_list_t * subscriber_list;
        wbt_subscriber_t * subscriber;
        wbt_mem_t tmp_mem;
        subscriber_list = wbt_list_first_entry(&channel->subscriber_list->list, wbt_subscriber_list_t, list);
        subscriber = subscriber_list->subscriber;
        
        if( wbt_malloc( &tmp_mem, sizeof(wbt_msg_list_t) ) == WBT_OK ) {
            if( wbt_list_empty(&subscriber->msg_list->list) &&
                subscriber->ev->on_timeout == NULL/* TODO */ ) {
                // TODO 添加 EPOLL_OUT 监听
            }

            wbt_msg_list_t * tmp_node = tmp_mem.ptr;
            tmp_node->msg = msg;
            wbt_list_add_tail( &tmp_node->list, &subscriber->msg_list->list );
            
            // 投递成功后，将该订阅者移动到链表末尾
            wbt_list_del(&subscriber_list->list);
            wbt_list_add_tail(&subscriber_list->list, &channel->subscriber_list->list);
        } else {
            // 内存不足，投递失败
            // TODO 需要记录该次投递失败
        }
    } else {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}