/* 
 * File:   wbt_mq_subscriber.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:21
 */

#include "wbt_mq_subscriber.h"

// 存储所有的订阅者
wbt_rbtree_t wbt_mq_subscribers;

wbt_subscriber_t * wbt_mq_subscriber_create() {
    static wbt_mq_id auto_inc_id = 0;
    wbt_subscriber_t * subscriber = wbt_new(wbt_subscriber_t);
    
    if( subscriber ) {
        subscriber->subscriber_id = ++auto_inc_id;
        subscriber->create = wbt_cur_mtime;
    }
    
    return subscriber;
}

void wbt_mq_subscriber_destory(wbt_subscriber_t *subscriber) {
    wbt_delete(subscriber);
}