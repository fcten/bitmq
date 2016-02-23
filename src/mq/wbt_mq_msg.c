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

static wbt_mq_id wbt_msg_create_count = 0;
static wbt_mq_id wbt_msg_effect_count = 0;
static wbt_mq_id wbt_msg_delete_count = 0;

wbt_msg_t * wbt_mq_msg_create() {
    wbt_msg_t * msg = wbt_calloc(sizeof(wbt_msg_t));
    
    if( msg ) {
        msg->msg_id = ++wbt_msg_create_count;
        msg->seq_id = 0;
        msg->create = wbt_cur_mtime;
        
        wbt_str_t msg_key;
        wbt_variable_to_str(msg->msg_id, msg_key);
        wbt_rbtree_node_t * msg_node = wbt_rbtree_insert(&wbt_mq_messages, &msg_key);
        if( msg_node == NULL ) {
            wbt_free(msg);
            return NULL;
        }
        msg_node->value.str = (char *)msg;
        
        wbt_log_debug("msg %lld create\n", msg->msg_id);
    }
    
    return msg;
}

wbt_msg_t * wbt_mq_msg_get(wbt_mq_id msg_id) {
    wbt_str_t msg_key;
    wbt_variable_to_str(msg_id, msg_key);
    wbt_rbtree_node_t * msg_node = wbt_rbtree_get(&wbt_mq_messages, &msg_key);
    if( msg_node == NULL ) {
        // TODO 调用持久化接口，从磁盘中读取并缓存到内存中
        return NULL;
    }
    return (wbt_msg_t *)msg_node->value.str;
}

void wbt_mq_msg_destory(wbt_msg_t *msg) {
    if( msg == NULL ) {
        return;
    }
    
    wbt_log_debug("msg %lld deleted\n", msg->msg_id);
    
    // 从消息所对应的频道中删除该消息
    // TODO 这里没有必要再做一次 wbt_mq_channel_get 查询
    wbt_channel_t * channel = wbt_mq_channel_get(msg->consumer_id);
    if( channel ) {
        wbt_mq_channel_del_msg(channel, msg);
    }
    
    if( msg->timeout_ev ) {
        wbt_event_del( msg->timeout_ev );
    }
    
    wbt_free(msg->data);

    wbt_str_t msg_key;
    wbt_variable_to_str(msg->msg_id, msg_key);
    wbt_rbtree_node_t * msg_node = wbt_rbtree_get( &wbt_mq_messages, &msg_key );
    if( msg_node ) {
        wbt_rbtree_delete( &wbt_mq_messages, msg_node );
    }
    
    wbt_msg_delete_count ++;
}

wbt_status wbt_mq_msg_destory_expired(wbt_event_t *ev) {
    wbt_mq_msg_destory( ev->ctx );
    
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
        
        msg->seq_id = ++wbt_msg_effect_count;
    }

    wbt_channel_t * channel = wbt_mq_channel_get(msg->consumer_id);
    if( channel == NULL ) {
        return WBT_ERROR;
    }

    // 已生效消息，保存该消息
    if( wbt_mq_channel_add_msg(channel, msg) != WBT_OK ) {
        return WBT_ERROR;
    }

    if( msg->delivery_mode == MSG_BROADCAST ) {
        // 广播模式，通知该频道下所有的订阅者获取该消息
        wbt_subscriber_list_t * subscriber_node;
        wbt_subscriber_t * subscriber;
        wbt_list_for_each_entry( subscriber_node, &channel->subscriber_list->head, head ) {
            subscriber = subscriber_node->subscriber;

            if( subscriber->ev->on_timeout == wbt_mq_pull_timeout ) {
                if( wbt_mq_subscriber_send_msg(subscriber) != WBT_OK ) {
                    // TODO 记录投递失败
                    wbt_log_debug("msg %lld send to %d failed\n", msg->msg_id, subscriber->ev->fd);
                }
            }
        }
    } else if( msg->delivery_mode == MSG_LOAD_BALANCE ) {
        if( wbt_list_empty( &channel->subscriber_list->head ) ) {
            // 频道没有任何订阅者
            return WBT_OK;
        }

        // 负载均衡模式，通知订阅队列中的第一个订阅者获取该消息
        wbt_subscriber_list_t * subscriber_node;
        wbt_subscriber_t * subscriber;
        subscriber_node = wbt_list_first_entry(&channel->subscriber_list->head, wbt_subscriber_list_t, head);
        subscriber = subscriber_node->subscriber;

        // TODO 这里忽略了投递失败的情况，导致消息不能严格地平均分发
        // 后续应当改进负载均衡算法
        if( subscriber->ev->on_timeout == wbt_mq_pull_timeout ) {
            if( wbt_mq_subscriber_send_msg(subscriber) != WBT_OK ) {
                // 直接投递失败，忽略该错误
            }
        }

        // 无论是否成功，都应当将该订阅者移动到链表末尾
        wbt_list_move_tail(&subscriber_node->head, &channel->subscriber_list->head);
    } else {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_msg_list_t * wbt_mq_msg_create_node(wbt_mq_id msg_id) {
    wbt_msg_list_t * msg_node = wbt_calloc(sizeof(wbt_msg_list_t));
    if( msg_node == NULL ) {
        return NULL;
    }
    msg_node->msg_id = msg_id;

    return msg_node;
}

void wbt_mq_msg_destory_node(wbt_msg_list_t *node) {
    wbt_free(node);
}

long long int wbt_mq_msg_status_total() {
    return (long long int)wbt_msg_create_count;
}

long long int wbt_mq_msg_status_active() {
    return (long long int)(wbt_msg_create_count - wbt_msg_delete_count);
}

long long int wbt_mq_msg_status_delayed() {
    return 0;
}

long long int wbt_mq_msg_status_waiting_ack() {
    return 0;
}

json_object_t * wbt_mq_msg_print(wbt_msg_t *msg) {
    json_object_t * obj = json_create_object();
    
    
    return obj;
}