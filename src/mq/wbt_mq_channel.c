/* 
 * File:   wbt_mq_channel.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:32
 */

#include "wbt_mq_channel.h"
#include "wbt_mq_msg.h"
#include "wbt_mq_subscriber.h"

// 存储所有可用频道
static wbt_rb_t wbt_mq_channels;

wbt_status wbt_mq_channel_init() {
    wbt_rb_init(&wbt_mq_channels, WBT_RB_KEY_LONGLONG);

    return WBT_OK;
}

wbt_channel_t * wbt_mq_channel_create(wbt_mq_id channel_id) {
    wbt_channel_t * channel = wbt_calloc(sizeof(wbt_channel_t));
    
    if( channel ) {
        channel->channel_id = channel_id;
        channel->create = wbt_cur_mtime;
        
        wbt_rb_init(&channel->queue, WBT_RB_KEY_LONGLONG);
        wbt_list_init(&channel->subscriber_list.head);
        
        wbt_str_t channel_key;
        wbt_variable_to_str(channel->channel_id, channel_key);
        wbt_rb_node_t * channel_node = wbt_rb_insert(&wbt_mq_channels, &channel_key);
        if( channel_node == NULL ) {
            wbt_free(channel);
            return NULL;
        }
        channel_node->value.str = (char *)channel;
    }
    
    return channel;
}

wbt_channel_t * wbt_mq_channel_get(wbt_mq_id channel_id) {
    wbt_channel_t * channel;
    wbt_str_t channel_key;
    wbt_variable_to_str(channel_id, channel_key);
    wbt_rb_node_t * channel_node = wbt_rb_get(&wbt_mq_channels, &channel_key);

    if( channel_node == NULL ) {
        channel = wbt_mq_channel_create(channel_id);
    } else {
        channel = (wbt_channel_t *)channel_node->value.str;
    }
    
    return channel;
}

void wbt_mq_channel_destory(wbt_channel_t *channel) {
    if( channel == NULL ) {
        return;
    }

    wbt_subscriber_list_t * pos;
    while(!wbt_list_empty(&channel->subscriber_list.head)) {
        pos = wbt_list_first_entry(&channel->subscriber_list.head, wbt_subscriber_list_t, head);
        wbt_list_del(&pos->head);
        wbt_free(pos);
    }
    
    wbt_rb_destroy_ignore_value(&channel->queue);

    wbt_str_t channel_key;
    wbt_variable_to_str(channel->channel_id, channel_key);
    wbt_rb_node_t * channel_node = wbt_rb_get( &wbt_mq_channels, &channel_key );
    if( channel_node ) {
        wbt_rb_delete( &wbt_mq_channels, channel_node );
    }
}

wbt_status wbt_mq_channel_add_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber) {
    wbt_subscriber_list_t *subscriber_node = wbt_calloc(sizeof(wbt_subscriber_list_t));
    if( subscriber_node == NULL ) {
        return WBT_ERROR;
    }
    subscriber_node->subscriber = subscriber;
    wbt_list_add(&subscriber_node->head, &channel->subscriber_list.head);
    
    channel->subscriber_count ++;
    
    return WBT_OK;
}

wbt_status wbt_mq_channel_del_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber) {
    // 从频道的 subscriber_list 中移除订阅者
    // TODO 这需要遍历链表，对于大量订阅者的频道来说不可接受。
    // 但是负载均衡模式又需要用到链表，这个地方还需要斟酌
    wbt_subscriber_list_t *subscriber_node;
    wbt_list_for_each_entry(subscriber_node, wbt_subscriber_list_t, &channel->subscriber_list.head, head) {
        if( subscriber_node->subscriber == subscriber ) {
            wbt_list_del(&subscriber_node->head);
            wbt_free(subscriber_node);
            
            channel->subscriber_count --;
            
            return WBT_OK;
        }
    }
    return WBT_OK;
}

/* 输出所有频道（默认最多输出 100 条） */
void wbt_mq_channel_print_all(json_object_t * obj) {
    int max = 100;
    wbt_rb_node_t *node;
    wbt_channel_t *channel;
      
    for (node = wbt_rb_first(&wbt_mq_channels); node; node = wbt_rb_next(node)) {
        channel = (wbt_channel_t *)node->value.str;
        json_append(obj, NULL, 0, JSON_OBJECT, wbt_mq_channel_print(channel), 0);
        if( --max <= 0 ) {
            break;
        }
    }
}

json_object_t * wbt_mq_channel_print(wbt_channel_t *channel) {
    json_object_t * obj = json_create_object();
    json_object_t * message = json_create_object();
    json_object_t * subscriber = json_create_object();
    json_append(obj, wbt_mq_str_channel_id.str, wbt_mq_str_channel_id.len, JSON_LONGLONG, &channel->channel_id, 0);
    json_append(obj, wbt_mq_str_message.str,    wbt_mq_str_message.len,    JSON_OBJECT,   message,              0);
    json_append(obj, wbt_mq_str_subscriber.str, wbt_mq_str_subscriber.len, JSON_OBJECT,   subscriber,           0);

    json_append(message, wbt_mq_str_total.str, wbt_mq_str_total.len, JSON_INTEGER, &channel->queue.size, 0 );
    
    json_append(subscriber, wbt_mq_str_total.str, wbt_mq_str_total.len, JSON_INTEGER, &channel->subscriber_count, 0 );
    
    return obj;
}

/* 输出指定频道的所有堆积消息（默认最多输出 100 条） */
void wbt_mq_channel_msg_print(wbt_channel_t *channel, json_object_t * obj) {
    int max = 100;
    wbt_rb_node_t *node;
    wbt_msg_t *msg;
      
    for (node = wbt_rb_first(&channel->queue); node; node = wbt_rb_next(node)) {
        msg = (wbt_msg_t *)node->value.str;
        json_append(obj, NULL, 0, JSON_LONGLONG, &msg->msg_id, 0);
        if( --max <= 0 ) {
            break;
        }
    }
}

/* 输出指定频道的所有订阅者（默认最多输出 100 条） */
void wbt_mq_channel_subscriber_print(wbt_channel_t *channel, json_object_t * obj) {
    int max = 100;
    if( !wbt_list_empty(&channel->subscriber_list.head) ) {
        wbt_subscriber_t * subscriber;
        wbt_subscriber_list_t * subscriber_node;
        wbt_list_for_each_entry( subscriber_node, wbt_subscriber_list_t, &channel->subscriber_list.head, head ) {
            subscriber = subscriber_node->subscriber;

            json_append(obj, NULL, 0, JSON_LONGLONG, &subscriber->subscriber_id, 0);

            if( --max <= 0 ) {
                break;
            }
        }
    }
}

long long int wbt_mq_channel_status_active() {
    return wbt_mq_channels.size;
}

wbt_status wbt_mq_channel_add_msg(wbt_channel_t *channel, wbt_msg_t *msg) {
    // 把消息记录到该频道的消息队列
    wbt_str_t key;
    wbt_variable_to_str(msg->seq_id, key);
    wbt_rb_node_t *node = wbt_rb_insert(&channel->queue, &key);
    if( node == NULL ) {
        // seq_id 重复或者内存不足
        return WBT_ERROR;
    }
    node->value.str = (char *)msg;
    
    // 将消息推送给频道内的订阅者
    if( msg->type == MSG_BROADCAST ) {
        // 广播模式
        wbt_subscriber_list_t * subscriber_node;
        wbt_subscriber_t * subscriber;
        wbt_list_for_each_entry( subscriber_node, wbt_subscriber_list_t, &channel->subscriber_list.head, head ) {
            subscriber = subscriber_node->subscriber;
            subscriber->notify(subscriber->ev);
        }
    } else if( msg->type == MSG_LOAD_BALANCE ) {
        if( wbt_list_empty( &channel->subscriber_list.head ) ) {
            // 频道没有任何订阅者
            return WBT_OK;
        }

        // 负载均衡模式，队列中的第一个订阅者获取该消息
        wbt_subscriber_list_t * subscriber_node;
        wbt_subscriber_t * subscriber;
        subscriber_node = wbt_list_first_entry(&channel->subscriber_list.head, wbt_subscriber_list_t, head);
        subscriber = subscriber_node->subscriber;
        subscriber->notify(subscriber->ev);

        // 将该订阅者移动到链表末尾
        wbt_list_move_tail(&subscriber_node->head, &channel->subscriber_list.head);
    }
    
    return WBT_OK;
}

void wbt_mq_channel_del_msg(wbt_channel_t *channel, wbt_msg_t *msg) {
    wbt_str_t key;
    wbt_variable_to_str(msg->seq_id, key);
    wbt_rb_node_t *node = wbt_rb_get(&channel->queue, &key);
    if( node == NULL ) {
        return;
    }
    
    node->value.str = NULL;
    wbt_rb_delete(&channel->queue, node);
}
