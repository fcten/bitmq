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
        subscriber = wbt_mq_subscriber_create(subscriber_id);
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

    wbt_msg_list_t * msg_node;
    while(!wbt_list_empty(&subscriber->delivered_list.head)) {
        msg_node = wbt_list_first_entry(&subscriber->delivered_list.head, wbt_msg_list_t, head);
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

/**
 * 向一个订阅者发送消息
 * @param subscriber
 * @param msg
 * @return 
 */
wbt_status wbt_mq_subscriber_send_msg(wbt_subscriber_t *subscriber) {
    // 遍历所订阅的频道，获取可投递消息
    wbt_channel_list_t *channel_node;
    wbt_rb_node_t *node;
    wbt_msg_t *msg = NULL;
    wbt_str_t key;
    wbt_list_for_each_entry(channel_node, wbt_channel_list_t, &subscriber->channel_list.head, head) {
        wbt_variable_to_str(channel_node->seq_id, key);
        node = wbt_rb_get_greater(&channel_node->channel->queue, &key);
        if( node ) {
            msg = (wbt_msg_t *)node->value.str;
            break;
        }
    }

    if(msg) {
        json_object_t *obj = wbt_mq_msg_print(msg);

        wbt_str_t resp;
        resp.len = 10240;
        resp.str = wbt_malloc( resp.len );
        if( resp.str == NULL ) {
            return WBT_ERROR;
        }        
        char *p = resp.str;
        size_t l = resp.len;
        json_print(obj, &p, &l);
        resp.len -= l;
        resp.str = wbt_realloc( resp.str, resp.len );
        
        json_delete_object(obj);
        
        if( subscriber->send( subscriber->ev, resp.str, resp.len, msg->qos, 0 ) != WBT_OK ) {
            wbt_free(resp.str);
            return WBT_ERROR;
        }
        wbt_free(resp.str);

        // 如果是负载均衡消息，将该消息移动到 delivered_list 中
        if( msg->type == MSG_LOAD_BALANCE ) {
            // 消息本身不能被释放
            node->value.str = NULL;
            wbt_rb_delete(&channel_node->channel->queue, node);

            wbt_msg_list_t *msg_node = wbt_mq_msg_create_node(msg->msg_id);
            if( msg_node == NULL ) {
                return WBT_ERROR;
            }
            wbt_list_add_tail( &msg_node->head, &subscriber->delivered_list.head );
        }

        wbt_mq_msg_inc_delivery(msg);

        // 更新消息处理进度
        channel_node->seq_id = msg->seq_id;

        return WBT_OK;
    }

    return WBT_ERROR;
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
