/* 
 * File:   wbt_mq_subscriber.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:21
 */

#include "wbt_mq_subscriber.h"
#include "wbt_mq_msg.h"

// 存储所有的订阅者
static wbt_rbtree_t wbt_mq_subscribers;

wbt_status wbt_mq_subscriber_init() {
    wbt_rbtree_init(&wbt_mq_subscribers);

    return WBT_OK;
}

wbt_subscriber_t * wbt_mq_subscriber_create() {
    static wbt_mq_id auto_inc_id = 0;
    wbt_subscriber_t * subscriber = wbt_new(wbt_subscriber_t);
    
    if( subscriber ) {
        subscriber->subscriber_id = ++auto_inc_id;
        subscriber->create = wbt_cur_mtime;
        subscriber->msg_list = wbt_new(wbt_msg_list_t);
        subscriber->channel_list = wbt_new(wbt_channel_list_t);
        subscriber->delivered_list = wbt_new(wbt_msg_list_t);
        
        if( subscriber->msg_list &&
            subscriber->channel_list &&
            subscriber->delivered_list ) {
            wbt_list_init(&subscriber->msg_list->head);
            wbt_list_init(&subscriber->channel_list->head);
            wbt_list_init(&subscriber->delivered_list->head);
        } else {
            wbt_delete(subscriber->msg_list);
            wbt_delete(subscriber->channel_list);
            wbt_delete(subscriber->delivered_list);
            wbt_delete(subscriber);
            return NULL;
        }

        wbt_str_t subscriber_key;
        wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
        wbt_rbtree_node_t * subscriber_node = wbt_rbtree_insert(&wbt_mq_subscribers, &subscriber_key);
        if( subscriber_node == NULL ) {
            wbt_delete(subscriber->msg_list);
            wbt_delete(subscriber->channel_list);
            wbt_delete(subscriber->delivered_list);
            wbt_delete(subscriber);
            return NULL;
        }
        subscriber_node->value.ptr = subscriber;
        subscriber_node->value.len = sizeof(wbt_subscriber_t);
    }
    
    return subscriber;
}

wbt_subscriber_t * wbt_mq_subscriber_get(wbt_mq_id subscriber_id) {
    wbt_subscriber_t * subscriber;
    wbt_str_t subscriber_key;
    wbt_variable_to_str(subscriber_id, subscriber_key);
    wbt_rbtree_node_t * subscriber_node = wbt_rbtree_get(&wbt_mq_subscribers, &subscriber_key);

    if( subscriber_node == NULL ) {
        subscriber = wbt_mq_subscriber_create(subscriber_id);
    } else {
        subscriber = subscriber_node->value.ptr;
    }
    
    return subscriber;
}

void wbt_mq_subscriber_destory(wbt_subscriber_t *subscriber) {
    if( subscriber == NULL ) {
        return;
    }

    if( subscriber->channel_list ) {
        wbt_channel_list_t * pos;
        while(!wbt_list_empty(&subscriber->channel_list->head)) {
            pos = wbt_list_first_entry(&subscriber->channel_list->head, wbt_channel_list_t, head);
            wbt_list_del(&pos->head);
            wbt_delete(pos);
        }
        wbt_delete(subscriber->channel_list);
    }

    if( subscriber->msg_list ) {
        wbt_msg_list_t * pos;
        while(!wbt_list_empty(&subscriber->msg_list->head)) {
            pos = wbt_list_first_entry(&subscriber->msg_list->head, wbt_msg_list_t, head);
            wbt_list_del(&pos->head);
            wbt_mq_msg_destory_node(pos);
        }
        wbt_delete(subscriber->msg_list);
    }

    if( subscriber->delivered_list ) {
        wbt_msg_list_t * pos;
        while(!wbt_list_empty(&subscriber->delivered_list->head)) {
            pos = wbt_list_first_entry(&subscriber->delivered_list->head, wbt_msg_list_t, head);
            wbt_list_del(&pos->head);
            wbt_mq_msg_dec_refer(pos->msg);
            wbt_mq_msg_destory_node(pos);
        }
        wbt_delete(subscriber->delivered_list);
    }
    
    wbt_str_t subscriber_key;
    wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
    wbt_rbtree_node_t * subscriber_node = wbt_rbtree_get( &wbt_mq_subscribers, &subscriber_key );
    if( subscriber_node ) {
        wbt_rbtree_delete( &wbt_mq_subscribers, subscriber_node );
    }
}

wbt_status wbt_mq_subscriber_add_channel(wbt_subscriber_t *subscriber, wbt_channel_t *channel) {
    wbt_channel_list_t *channel_node = wbt_new(wbt_channel_list_t);
    if( channel_node == NULL ) {
        return WBT_ERROR;
    }
    channel_node->channel = channel;
    wbt_list_add(&channel_node->head, &subscriber->channel_list->head);
    
    return WBT_OK;
}

/**
 * 向一个正处于 pull 阻塞状态的订阅者发送消息
 * @param subscriber
 * @param msg
 * @return 
 */
wbt_status wbt_mq_subscriber_send_msg(wbt_subscriber_t *subscriber, wbt_msg_t *msg) {
    wbt_mem_t tmp;
    if( wbt_malloc(&tmp, msg->data.len) != WBT_OK ) {
        return WBT_ERROR;
    }
    wbt_memcpy(&tmp, &msg->data, msg->data.len);

    wbt_http_t * tmp_http = subscriber->ev->data.ptr;

    tmp_http->status = STATUS_200;
    tmp_http->file.ptr = tmp.ptr;
    tmp_http->file.size = tmp.len;

    if( wbt_http_process(subscriber->ev) != WBT_OK ) {
        // 内存不足，投递失败
        wbt_conn_close(subscriber->ev);
        return WBT_ERROR;
    } else {
        tmp_http->state = STATE_SENDING;

        /* 等待socket可写 */
        subscriber->ev->on_timeout = wbt_conn_close;
        subscriber->ev->on_send = wbt_on_send;
        subscriber->ev->events = EPOLLOUT | EPOLLET;
        subscriber->ev->timeout = wbt_cur_mtime + wbt_conf.event_timeout;

        if(wbt_event_mod(subscriber->ev) != WBT_OK) {
            return WBT_ERROR;
        }
        
        subscriber->msg = msg;
        wbt_mq_msg_inc_refer(subscriber->msg);
    }

    return WBT_OK;
}