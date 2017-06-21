/* 
 * File:   wbt_mq_subscriber.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:21
 */

#include "wbt_mq_subscriber.h"
#include "wbt_mq_msg.h"

// 存储所有的订阅者
static wbt_rb_t wbt_mq_subscribers;

wbt_status wbt_mq_subscriber_init() {
    wbt_rb_init(&wbt_mq_subscribers, WBT_RB_KEY_LONGLONG);

    return WBT_OK;
}

wbt_subscriber_t * wbt_mq_subscriber_create() {
    static wbt_mq_id auto_inc_id = 0;
    wbt_subscriber_t * subscriber = wbt_calloc(sizeof(wbt_subscriber_t));
    
    if( subscriber ) {
        subscriber->subscriber_id = ++auto_inc_id;
        subscriber->create = wbt_cur_mtime;

        wbt_list_init(&subscriber->channel_list.head);
        wbt_list_init(&subscriber->delivered_list.head);

        wbt_str_t subscriber_key;
        wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
        wbt_rb_node_t * subscriber_node = wbt_rb_insert(&wbt_mq_subscribers, &subscriber_key);
        if( subscriber_node == NULL ) {
            wbt_free(subscriber);
            return NULL;
        }
        subscriber_node->value.str = (char *)subscriber;
    }
    
    return subscriber;
}

wbt_subscriber_t * wbt_mq_subscriber_get(wbt_mq_id subscriber_id) {
    wbt_subscriber_t * subscriber;
    wbt_str_t subscriber_key;
    wbt_variable_to_str(subscriber_id, subscriber_key);
    wbt_rb_node_t * subscriber_node = wbt_rb_get(&wbt_mq_subscribers, &subscriber_key);

    if( subscriber_node == NULL ) {
        return NULL;
    } else {
        subscriber = (wbt_subscriber_t *)subscriber_node->value.str;
    }
    
    return subscriber;
}

void wbt_mq_subscriber_destory(wbt_subscriber_t *subscriber) {
    if( subscriber == NULL ) {
        return;
    }

    wbt_channel_list_t * channel_node;
    while(!wbt_list_empty(&subscriber->channel_list.head)) {
        channel_node = wbt_list_first_entry(&subscriber->channel_list.head, wbt_channel_list_t, head);
        wbt_list_del(&channel_node->head);
        wbt_free(channel_node);
    }

    // 重新投递尚未返回 ACK 响应的负载均衡消息
    wbt_msg_list_t * msg_node;
    wbt_msg_t *msg;
    while(!wbt_list_empty(&subscriber->delivered_list.head)) {
        msg_node = wbt_list_first_entry(&subscriber->delivered_list.head, wbt_msg_list_t, head);
        msg = wbt_mq_msg_get(msg_node->msg_id);
        
        if(msg) {
            msg->state = MSG_CREATED;
            wbt_mq_msg_timer_add(msg);
            
            wbt_log_debug("msg_id %lld: %ld\n", msg->msg_id, msg->effect);
        }
        
        wbt_list_del(&msg_node->head);
        wbt_mq_msg_destory_node(msg_node);
    }
    
    wbt_str_t subscriber_key;
    wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
    wbt_rb_node_t * subscriber_node = wbt_rb_get( &wbt_mq_subscribers, &subscriber_key );
    if( subscriber_node ) {
        wbt_rb_delete( &wbt_mq_subscribers, subscriber_node );
    }
}

// TODO 如果不严格限制订阅数量则需要优化
int wbt_mq_subscriber_has_channel(wbt_subscriber_t *subscriber, wbt_channel_t *channel) {
    wbt_channel_list_t *channel_node;
    wbt_list_for_each_entry(channel_node, wbt_channel_list_t, &subscriber->channel_list.head, head) {
        if( channel == channel_node->channel ) {
            return 1;
        }
    }
    
    return 0;
}

wbt_status wbt_mq_subscriber_add_channel(wbt_subscriber_t *subscriber, wbt_channel_t *channel) {
    wbt_channel_list_t *channel_node = wbt_calloc(sizeof(wbt_channel_list_t));
    if( channel_node == NULL ) {
        return WBT_ERROR;
    }
    channel_node->channel = channel;
    wbt_list_add(&channel_node->head, &subscriber->channel_list.head);
    
    return WBT_OK;
}

wbt_status wbt_mq_subscriber_msg_ack(wbt_subscriber_t *subscriber, wbt_mq_id msg_id) {
    wbt_msg_list_t *msg_node;
    wbt_msg_t *msg;
    wbt_list_for_each_entry(msg_node, wbt_msg_list_t, &subscriber->delivered_list.head, head) {
        if( msg_node->msg_id == msg_id ) {
            msg = wbt_mq_msg_get(msg_node->msg_id);
            if( msg ) {
                wbt_mq_msg_destory(msg);
            }

            wbt_list_del(&msg_node->head);
            wbt_mq_msg_destory_node(msg_node);

            return WBT_OK;
        }
    }
    
    return WBT_ERROR;
}

long long int wbt_mq_subscriber_status_active() {
    return wbt_mq_subscribers.size;
}

void wbt_mq_subscriber_print_all(json_object_t * obj) {
    int max = 100;
    wbt_rb_node_t *node;
    wbt_subscriber_t *subscriber;
      
    for (node = wbt_rb_first(&wbt_mq_subscribers); node; node = wbt_rb_next(node)) {
        subscriber = (wbt_subscriber_t *)node->value.str;
        json_append(obj, NULL, 0, JSON_OBJECT, wbt_mq_subscriber_print(subscriber), 0);
        if( --max <= 0 ) {
            break;
        }
    }
}

json_object_t * wbt_mq_subscriber_print(wbt_subscriber_t *subscriber) {
    json_object_t * obj = json_create_object();

    json_append(obj, wbt_mq_str_subscriber_id.str, wbt_mq_str_subscriber_id.len, JSON_LONGLONG, &subscriber->subscriber_id, 0);
    json_append(obj, wbt_mq_str_create.str,        wbt_mq_str_create.len,        JSON_LONGLONG, &subscriber->create       , 0);

    return obj;
}

void wbt_mq_subscriber_msg_print(wbt_subscriber_t *subscriber, json_object_t * obj) {
    int max = 100;
    if( !wbt_list_empty(&subscriber->delivered_list.head) ) {
        wbt_msg_list_t * msg_node;
        wbt_list_for_each_entry( msg_node, wbt_msg_list_t, &subscriber->delivered_list.head, head ) {
            json_append(obj, NULL, 0, JSON_LONGLONG, &msg_node->msg_id, 0);

            if( --max <= 0 ) {
                break;
            }
        }
    }
}

void wbt_mq_subscriber_channel_print(wbt_subscriber_t *subscriber, json_object_t * obj) {
    int max = 100;
    if( !wbt_list_empty(&subscriber->channel_list.head) ) {
        wbt_channel_t * channel;
        wbt_channel_list_t * channel_node;
        wbt_list_for_each_entry( channel_node, wbt_channel_list_t, &subscriber->channel_list.head, head ) {
            channel = channel_node->channel;

            json_append(obj, NULL, 0, JSON_LONGLONG, &channel->channel_id, 0);

            if( --max <= 0 ) {
                break;
            }
        }
    }
}