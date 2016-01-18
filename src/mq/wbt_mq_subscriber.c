/* 
 * File:   wbt_mq_subscriber.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:21
 */

#include "wbt_mq_subscriber.h"

// 存储所有的订阅者
wbt_rbtree_t wbt_mq_subscribers;

wbt_subscriber_t * wbt_mq_subscriber_create(wbt_mq_id subscriber_id) {
    wbt_subscriber_t * subscriber = wbt_new(wbt_subscriber_t);
    
    if( subscriber ) {
        subscriber->subscriber_id = subscriber_id;
        subscriber->create = wbt_cur_mtime;

        wbt_str_t subscriber_key;
        wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
        wbt_rbtree_node_t * subscriber_node = wbt_rbtree_insert(&wbt_mq_subscribers, &subscriber_key);
        if( subscriber_node == NULL ) {
            wbt_delete(subscriber);
            return NULL;
        }
    }
    
    return subscriber;
}

void wbt_mq_subscriber_destory(wbt_subscriber_t *subscriber) {
    wbt_delete(subscriber);
}