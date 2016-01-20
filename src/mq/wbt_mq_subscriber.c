/* 
 * File:   wbt_mq_subscriber.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:21
 */

#include "wbt_mq_subscriber.h"

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
        
        if( subscriber->msg_list && subscriber->channel_list ) {
            wbt_list_init(&subscriber->msg_list->head);
            wbt_list_init(&subscriber->channel_list->head);
        } else {
            wbt_mq_subscriber_destory(subscriber);
            return NULL;
        }

        wbt_str_t subscriber_key;
        wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
        wbt_rbtree_node_t * subscriber_node = wbt_rbtree_insert(&wbt_mq_subscribers, &subscriber_key);
        if( subscriber_node == NULL ) {
            wbt_mq_subscriber_destory(subscriber);
            return NULL;
        }
        subscriber_node->value.ptr = subscriber;
        subscriber_node->value.len = sizeof(subscriber);
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
            wbt_delete(pos);
        }
        wbt_delete(subscriber->msg_list);
    }

    wbt_str_t subscriber_key;
    wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
    wbt_rbtree_node_t * subscriber_node = wbt_rbtree_get( &wbt_mq_subscribers, &subscriber_key );
    if( subscriber_node ) {
        wbt_rbtree_delete( &wbt_mq_subscribers, subscriber_node );
    }
}