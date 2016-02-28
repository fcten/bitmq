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
    wbt_subscriber_t * subscriber = wbt_calloc(sizeof(wbt_subscriber_t));
    
    if( subscriber ) {
        subscriber->subscriber_id = ++auto_inc_id;
        subscriber->create = wbt_cur_mtime;
        subscriber->channel_list = wbt_calloc(sizeof(wbt_channel_list_t));
        subscriber->delivered_list = wbt_mq_msg_create_node(0);
        
        if( subscriber->channel_list &&
            subscriber->delivered_list ) {
            wbt_list_init(&subscriber->channel_list->head);
            wbt_list_init(&subscriber->delivered_list->head);
        } else {
            wbt_free(subscriber->channel_list);
            wbt_free(subscriber->delivered_list);
            wbt_free(subscriber);
            return NULL;
        }

        wbt_str_t subscriber_key;
        wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
        wbt_rbtree_node_t * subscriber_node = wbt_rbtree_insert(&wbt_mq_subscribers, &subscriber_key);
        if( subscriber_node == NULL ) {
            wbt_free(subscriber->channel_list);
            wbt_free(subscriber->delivered_list);
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
    wbt_rbtree_node_t * subscriber_node = wbt_rbtree_get(&wbt_mq_subscribers, &subscriber_key);

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

    if( subscriber->channel_list ) {
        wbt_channel_list_t * pos;
        while(!wbt_list_empty(&subscriber->channel_list->head)) {
            pos = wbt_list_first_entry(&subscriber->channel_list->head, wbt_channel_list_t, head);
            wbt_list_del(&pos->head);
            wbt_free(pos);
        }
        wbt_free(subscriber->channel_list);
    }

    if( subscriber->delivered_list ) {
        wbt_msg_list_t * pos;
        while(!wbt_list_empty(&subscriber->delivered_list->head)) {
            pos = wbt_list_first_entry(&subscriber->delivered_list->head, wbt_msg_list_t, head);
            wbt_list_del(&pos->head);
            wbt_mq_msg_destory_node(pos);
        }
        wbt_free(subscriber->delivered_list);
    }
    
    wbt_str_t subscriber_key;
    wbt_variable_to_str(subscriber->subscriber_id, subscriber_key);
    wbt_rbtree_node_t * subscriber_node = wbt_rbtree_get( &wbt_mq_subscribers, &subscriber_key );
    if( subscriber_node ) {
        wbt_rbtree_delete( &wbt_mq_subscribers, subscriber_node );
    }
}

wbt_status wbt_mq_subscriber_add_channel(wbt_subscriber_t *subscriber, wbt_channel_t *channel) {
    wbt_channel_list_t *channel_node = wbt_calloc(sizeof(wbt_channel_list_t));
    if( channel_node == NULL ) {
        return WBT_ERROR;
    }
    channel_node->channel = channel;
    wbt_list_add(&channel_node->head, &subscriber->channel_list->head);
    
    return WBT_OK;
}

/**
 * 向一个订阅者发送消息
 * @param subscriber
 * @param msg
 * @return 
 */
wbt_status wbt_mq_subscriber_send_msg(wbt_subscriber_t *subscriber) {
    wbt_http_t * http = subscriber->ev->data;

    // 遍历所订阅的频道，获取可投递消息
    wbt_channel_list_t *channel_node;
    wbt_rbtree_node_t *node;
    wbt_msg_t *msg = NULL;
    wbt_str_t key;
    wbt_list_for_each_entry(channel_node, &subscriber->channel_list->head, head) {
        wbt_variable_to_str(channel_node->seq_id, key);
        node = wbt_rbtree_get_greater(&channel_node->channel->queue, &key);
        if( node ) {
            msg = (wbt_msg_t *)node->value.str;
            break;
        }
    }

    if(msg) {
        http->resp_body_memory.str = wbt_strdup(msg->data, msg->data_len);
        if( http->resp_body_memory.str == NULL ) {
            return WBT_ERROR;
        }
        http->resp_body_memory.len = msg->data_len;
        
        http->status = STATUS_200;
        http->state = STATE_SENDING;
        
        if( wbt_http_process(subscriber->ev) != WBT_OK ) {
            // 这个方法被调用时，可能正在遍历订阅者链表，这时不能关闭连接
            // TODO 以合适的方法关闭连接
            return WBT_ERROR;
        }

        /* 等待socket可写 */
        subscriber->ev->timer.on_timeout = wbt_conn_close;
        subscriber->ev->timer.timeout    = wbt_cur_mtime + wbt_conf.event_timeout;
        subscriber->ev->on_send = wbt_on_send;
        subscriber->ev->events  = EPOLLOUT | EPOLLET;

        if(wbt_event_mod(subscriber->ev) != WBT_OK) {
            return WBT_ERROR;
        }

        // 如果是负载均衡消息，则从该频道中暂时移除该消息，以被免重复处理
        if( msg->delivery_mode == MSG_LOAD_BALANCE ) {
            // 消息本身不能被释放
            node->value.str = NULL;
            wbt_rbtree_delete(&channel_node->channel->queue, node);
        }

        // 更新消息处理进度
        channel_node->seq_id = msg->seq_id;
        
        // 保存当前正在处理的消息指针
        subscriber->msg_id = msg->msg_id;

        return WBT_OK;
    }

    return WBT_ERROR;
}

long long int wbt_mq_subscriber_status_active() {
    return wbt_mq_subscribers.size;
}