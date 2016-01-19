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

// 存储所有未生效的消息
wbt_heap_t wbt_mq_message_delayed;

wbt_msg_t * wbt_mq_msg_create() {
    static wbt_mq_id auto_inc_id = 0;
    wbt_msg_t * msg = wbt_new(wbt_msg_t);
    
    if( msg ) {
        msg->msg_id = ++auto_inc_id;
        msg->create = wbt_cur_mtime;
        
        wbt_str_t msg_key;
        wbt_variable_to_str(msg->msg_id, msg_key);
        wbt_rbtree_node_t * msg_node = wbt_rbtree_insert(&wbt_mq_messages, &msg_key);
        if( msg_node == NULL ) {
            wbt_delete(msg);
            return NULL;
        }
    }
    
    return msg;
}

void wbt_mq_msg_destory(wbt_msg_t *msg) {
    wbt_delete(msg);
}

/**
 * 投递一条消息
 * @param msg 消息体指针
 * @return wbt_status
 */
wbt_status wbt_mq_msg_delivery(wbt_msg_t *msg) {
    wbt_str_t channel_id;
    wbt_variable_to_str(msg->consumer_id, channel_id);
    wbt_channel_t * channel = wbt_rbtree_get_value(&wbt_mq_channels, &channel_id);
    if( channel == NULL ) {
        // 投递的频道不存在，投递失败
        // TODO 新建频道？
        return WBT_ERROR;
    }

    if( msg->expire <= wbt_cur_mtime ) {
        // 不应当尝试投递已过期的消息
        return WBT_ERROR;
    }

    wbt_heap_node_t * msg_node = wbt_new(wbt_heap_node_t);
    if( msg_node == NULL ) {
        return WBT_ERROR;
    }
    msg_node->ptr = msg;
    
    if( msg->effect > wbt_cur_mtime ) {
        // 未生效消息
        msg_node->timeout = msg->effect;
        // 保存该消息
        if( wbt_heap_insert(&wbt_mq_message_delayed, msg_node) != WBT_OK ) {
            wbt_delete(msg_node);
            return WBT_ERROR;
        }
        return WBT_OK;
    } else {
        // 已生效消息
        msg_node->timeout = msg->expire;
    }
    
    if( msg->delivery_mode == MSG_BROADCAST ) {
        // 保存该消息
        if( wbt_heap_insert(&channel->effective_heap, msg_node) != WBT_OK ) {
            wbt_delete(msg_node);
            return WBT_ERROR;
        }

        // 广播模式，消息将投递给该频道下所有的订阅者
        wbt_subscriber_list_t * subscriber_node;
        wbt_subscriber_t * subscriber;
        wbt_list_for_each_entry( subscriber_node, &channel->subscriber_list->head, head ) {
            subscriber = subscriber_node->subscriber;
            
            wbt_msg_list_t * tmp_node = wbt_new(wbt_msg_list_t);
            if( tmp_node == NULL ) {
                // 内存不足，投递失败
                // TODO 需要记录该次投递失败
                continue;
            }

            if( wbt_list_empty(&subscriber->msg_list->head) &&
                subscriber->ev->on_timeout == NULL/* TODO */ ) {
                // TODO 添加 EPOLL_OUT 监听
            }

            tmp_node->msg = msg;
            wbt_list_add_tail( &tmp_node->head, &subscriber->msg_list->head );
        }
    } else if( msg->delivery_mode == MSG_LOAD_BALANCE ) {
        if( wbt_list_empty( &channel->subscriber_list->head ) ) {
            // 频道没有任何订阅者
            if( wbt_heap_insert(&channel->effective_heap, msg_node) != WBT_OK ) {
                wbt_delete(msg_node);
                return WBT_ERROR;
            }
            return WBT_OK;
        }
        
        // 负载均衡模式，消息将投递给订阅队列中的第一个订阅者
        wbt_subscriber_list_t * subscriber_list;
        wbt_subscriber_t * subscriber;
        subscriber_list = wbt_list_first_entry(&channel->subscriber_list->head, wbt_subscriber_list_t, head);
        subscriber = subscriber_list->subscriber;

        wbt_msg_list_t * tmp_node = wbt_new(wbt_msg_list_t);
        if( tmp_node == NULL ) {
            // 内存不足，暂时无法投递
            if( wbt_heap_insert(&channel->effective_heap, msg_node) != WBT_OK ) {
                wbt_delete(msg_node);
                return WBT_ERROR;
            }
            return WBT_OK;
        }

        // 保存该消息
        if( wbt_heap_insert(&channel->delivered_heap, msg_node) != WBT_OK ) {
            wbt_delete(msg_node);
            return WBT_ERROR;
        }

        if( wbt_list_empty(&subscriber->msg_list->head) &&
            subscriber->ev->on_timeout == NULL/* TODO */ ) {
            // TODO 添加 EPOLL_OUT 监听
        }

        tmp_node->msg = msg;
        wbt_list_add_tail( &tmp_node->head, &subscriber->msg_list->head );

        // 投递成功后，将该订阅者移动到链表末尾
        wbt_list_move_tail(&subscriber_list->head, &channel->subscriber_list->head);
    } else {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

/**
 * 从最小堆定时器中删除掉超时消息
 * @param msg_heap
 */
void wbt_mq_remove_timeout_msg(wbt_heap_t * msg_heap) {
    wbt_heap_node_t * p = wbt_heap_get(msg_heap);
    while( msg_heap->size > 0 && p->timeout <= wbt_cur_mtime ) {
        wbt_heap_delete( msg_heap );
        p = wbt_heap_get(msg_heap);
    }
}