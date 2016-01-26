/* 
 * File:   wbt_mq_msg.c
 * Author: fcten
 *
 * Created on 2016年1月18日, 下午4:08
 */

#include "wbt_mq_msg.h"
#include "wbt_mq_channel.h"
#include "wbt_mq_subscriber.h"

// 存储所有已接收到的消息
static wbt_rbtree_t wbt_mq_messages;

wbt_status wbt_mq_msg_init() {
    wbt_rbtree_init(&wbt_mq_messages);

    return WBT_OK;
}

wbt_mq_id wbt_msg_create_count = 0;
wbt_mq_id wbt_msg_delete_count = 0;

wbt_msg_t * wbt_mq_msg_create() {
    wbt_msg_t * msg = wbt_new(wbt_msg_t);
    
    if( msg ) {
        msg->msg_id = ++wbt_msg_create_count;
        msg->create = wbt_cur_mtime;
        
        wbt_str_t msg_key;
        wbt_variable_to_str(msg->msg_id, msg_key);
        wbt_rbtree_node_t * msg_node = wbt_rbtree_insert(&wbt_mq_messages, &msg_key);
        if( msg_node == NULL ) {
            wbt_delete(msg);
            return NULL;
        }
        msg_node->value.ptr = msg;
        msg_node->value.len = sizeof(wbt_msg_t);
        
        wbt_log_debug("msg %lld create", msg->msg_id);
    }
    
    return msg;
}

void wbt_mq_msg_destory(wbt_msg_t *msg) {
    if( msg == NULL ) {
        return;
    }
    
    wbt_log_debug("msg %lld deleted", msg->msg_id);
    
    if( msg->timeout_ev ) {
        wbt_event_del( msg->timeout_ev );
    }
    
    wbt_free(&msg->data);

    wbt_str_t msg_key;
    wbt_variable_to_str(msg->msg_id, msg_key);
    wbt_rbtree_node_t * msg_node = wbt_rbtree_get( &wbt_mq_messages, &msg_key );
    if( msg_node ) {
        wbt_rbtree_delete( &wbt_mq_messages, msg_node );
    }
    
    wbt_msg_delete_count ++;
}

wbt_status wbt_mq_msg_destory_expired(wbt_event_t *ev) {
    wbt_msg_t *msg = ev->ctx;
    
    if( msg->reference_count == 0 ) {
        wbt_mq_msg_destory( msg );
    } else {
        msg->timeout_ev->timeout = wbt_cur_mtime + 10000;

        if(wbt_event_mod(msg->timeout_ev) != WBT_OK) {
            // 重新注册超时事件失败，该消息将无法得到释放。
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_msg_delivery_delayed(wbt_event_t *ev) {
    wbt_msg_t *msg = ev->ctx;
    
    wbt_event_del( msg->timeout_ev );
    msg->timeout_ev = NULL;
    
    if( wbt_mq_msg_delivery( msg ) == WBT_OK ) {
        return WBT_OK;
    } else {
        // 投递失败，该消息将会丢失。内存不足时可能出现该情况
        wbt_mq_msg_destory( msg );
        return WBT_ERROR;
    }
}

/**
 * 投递一条消息
 * @param msg 消息体指针
 * @return wbt_status
 */
wbt_status wbt_mq_msg_delivery(wbt_msg_t *msg) {
    if( msg->expire <= wbt_cur_mtime ) {
        // 投递的消息已经过期
        
        return WBT_OK;
    } else if( msg->effect > wbt_cur_mtime ) {
        // 未生效消息
        wbt_event_t tmp_ev;
        tmp_ev.on_timeout = wbt_mq_msg_delivery_delayed;
        tmp_ev.fd = -1;
        tmp_ev.timeout = msg->effect;
        if((msg->timeout_ev = wbt_event_add(&tmp_ev)) == NULL) {
            return WBT_ERROR;
        }
        msg->timeout_ev->ctx = msg;
        return WBT_OK;
    } else {
        // 已生效消息
        if( !msg->timeout_ev ) {
            wbt_event_t tmp_ev;
            tmp_ev.on_timeout = wbt_mq_msg_destory_expired;
            tmp_ev.fd = -1;
            tmp_ev.timeout = msg->expire;
            if((msg->timeout_ev = wbt_event_add(&tmp_ev)) == NULL) {
                return WBT_ERROR;
            }
            msg->timeout_ev->ctx = msg;
        }
    }

    wbt_channel_t * channel = wbt_mq_channel_get(msg->consumer_id);
    if( channel == NULL ) {
        return WBT_ERROR;
    }

    // 已生效消息
    wbt_msg_list_t * msg_node = wbt_mq_msg_create_node(msg);
    if( msg_node == NULL ) {
        return WBT_ERROR;
    }
    
    if( msg->delivery_mode == MSG_BROADCAST ) {
        // 保存该消息
        wbt_list_add_tail( &msg_node->head, &channel->msg_list->head );

        // 广播模式，消息将投递给该频道下所有的订阅者
        wbt_subscriber_list_t * subscriber_node;
        wbt_subscriber_t * subscriber;
        wbt_list_for_each_entry( subscriber_node, &channel->subscriber_list->head, head ) {
            subscriber = subscriber_node->subscriber;

            if( wbt_list_empty(&subscriber->msg_list->head) &&
                subscriber->ev->on_timeout == wbt_mq_pull_timeout ) {
                if( wbt_mq_subscriber_send_msg(subscriber, msg) != WBT_OK ) {
                    // TODO 记录投递失败
                }
            } else {
                wbt_msg_list_t * tmp_node = wbt_mq_msg_create_node(msg);
                if( tmp_node == NULL ) {
                    // 内存不足，投递失败
                    // TODO 需要记录该次投递失败
                } else {
                    wbt_list_add_tail( &tmp_node->head, &subscriber->msg_list->head );
                }
            }
        }
    } else if( msg->delivery_mode == MSG_LOAD_BALANCE ) {
        if( wbt_list_empty( &channel->subscriber_list->head ) ) {
            // 频道没有任何订阅者
            wbt_list_add_tail( &msg_node->head, &channel->msg_list->head );
            return WBT_OK;
        }
        
        // 负载均衡模式，消息将投递给订阅队列中的第一个订阅者
        wbt_subscriber_list_t * subscriber_list;
        wbt_subscriber_t * subscriber;
        subscriber_list = wbt_list_first_entry(&channel->subscriber_list->head, wbt_subscriber_list_t, head);
        subscriber = subscriber_list->subscriber;

        if( wbt_list_empty(&subscriber->msg_list->head) &&
            subscriber->ev->on_timeout == wbt_mq_pull_timeout ) {
            if( wbt_mq_subscriber_send_msg(subscriber, msg) != WBT_OK ) {
                //  投递失败
                wbt_list_add_tail( &msg_node->head, &channel->msg_list->head );
                return WBT_OK;
            }
            wbt_mq_msg_destory_node(msg_node);
        } else {
            wbt_list_add_tail( &msg_node->head, &subscriber->msg_list->head );
        }
        // 投递成功后，将该订阅者移动到链表末尾
        wbt_list_move_tail(&subscriber_list->head, &channel->subscriber_list->head);
    } else {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_msg_list_t * wbt_mq_msg_create_node(wbt_msg_t *msg) {
    wbt_msg_list_t * msg_node = wbt_new(wbt_msg_list_t);
    if( msg_node == NULL ) {
        return NULL;
    }
    msg_node->msg = msg;
    msg_node->expire = msg->expire;

    return msg_node;
}

void wbt_mq_msg_destory_node(wbt_msg_list_t *node) {
    wbt_delete(node);
}