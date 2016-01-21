/* 
 * File:   wbt_mq_msg.c
 * Author: fcten
 *
 * Created on 2016年1月18日, 下午4:08
 */

#include "wbt_mq_msg.h"
#include "wbt_mq_channel.h"

// 存储所有已接收到的消息
static wbt_rbtree_t wbt_mq_messages;

wbt_status wbt_mq_msg_init() {
    wbt_rbtree_init(&wbt_mq_messages);

    return WBT_OK;
}

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
        msg_node->value.ptr = msg;
        msg_node->value.len = sizeof(wbt_msg_t);
    }
    
    return msg;
}

void wbt_mq_msg_destory(wbt_msg_t *msg) {
    if( msg == NULL ) {
        return;
    }

    wbt_str_t msg_key;
    wbt_variable_to_str(msg->msg_id, msg_key);
    wbt_rbtree_node_t * msg_node = wbt_rbtree_get( &wbt_mq_messages, &msg_key );
    if( msg_node ) {
        wbt_rbtree_delete( &wbt_mq_messages, msg_node );
    }
}

wbt_status wbt_mq_msg_delivery_delayed(wbt_event_t *ev) {
    if( wbt_mq_msg_delivery( ev->ctx ) == WBT_OK ) {
        return WBT_OK;
    } else {
        // 投递失败，该消息将会丢失。内存不足时可能出现该情况
        wbt_mq_msg_destory( ev->ctx );
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
        wbt_event_t tmp_ev, *new_ev;
        tmp_ev.on_timeout = wbt_mq_msg_delivery_delayed;
        tmp_ev.fd = -1;
        tmp_ev.timeout = msg->effect;
        if((new_ev = wbt_event_add(&tmp_ev)) == NULL) {
            return WBT_ERROR;
        }
        new_ev->ctx = msg;
        return WBT_OK;
    }

    wbt_channel_t * channel = wbt_mq_channel_get(msg->consumer_id);
    if( channel == NULL ) {
        return WBT_ERROR;
    }

    // 已生效消息
    wbt_msg_list_t * msg_node = wbt_new(wbt_msg_list_t);
    if( msg_node == NULL ) {
        return WBT_ERROR;
    }
    msg_node->msg = msg;
    
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
                wbt_mem_t tmp;
                wbt_malloc(&tmp, msg->data.len);
                wbt_memcpy(&tmp, &msg->data, msg->data.len);

                wbt_http_t * tmp_http = subscriber->ev->data.ptr;

                tmp_http->status = STATUS_200;
                tmp_http->file.ptr = tmp.ptr;
                tmp_http->file.size = tmp.len;

                if( wbt_http_process(subscriber->ev) != WBT_OK ) {
                    // 内存不足，投递失败
                } else {
                    tmp_http->state = STATE_SENDING;

                    /* 等待socket可写 */
                    subscriber->ev->on_timeout = wbt_conn_close; // TODO 这个事件超时或发送失败意味着消息会丢失
                    subscriber->ev->on_send = wbt_on_send;
                    subscriber->ev->events = EPOLLOUT | EPOLLET;
                    subscriber->ev->timeout = wbt_cur_mtime + wbt_conf.event_timeout;

                    if(wbt_event_mod(subscriber->ev) != WBT_OK) {
                        return WBT_ERROR;
                    }
                }
            } else {
                wbt_msg_list_t * tmp_node = wbt_new(wbt_msg_list_t);
                if( tmp_node == NULL ) {
                    // 内存不足，投递失败
                    // TODO 需要记录该次投递失败
                } else {
                    tmp_node->msg = msg;
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

        wbt_msg_list_t * tmp_node = wbt_new(wbt_msg_list_t);
        if( tmp_node == NULL ) {
            // 内存不足，暂时无法投递
            wbt_list_add_tail( &msg_node->head, &channel->msg_list->head );
            return WBT_OK;
        }

        if( wbt_list_empty(&subscriber->msg_list->head) &&
            subscriber->ev->on_timeout == wbt_mq_pull_timeout ) {
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
