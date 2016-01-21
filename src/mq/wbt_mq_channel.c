/* 
 * File:   wbt_mq_channel.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:32
 */

#include "wbt_mq_channel.h"

// 存储所有可用频道
static wbt_rbtree_t wbt_mq_channels;

wbt_status wbt_mq_channel_init() {
    wbt_rbtree_init(&wbt_mq_channels);

    return WBT_OK;
}

wbt_channel_t * wbt_mq_channel_create(wbt_mq_id channel_id) {
    wbt_channel_t * channel = wbt_new(wbt_channel_t);
    
    if( channel ) {
        channel->channel_id = channel_id;
        channel->create = wbt_cur_mtime;

        channel->subscriber_list = wbt_new(wbt_subscriber_list_t);
        channel->msg_list = wbt_new(wbt_msg_list_t);
        if( channel->subscriber_list && channel->msg_list ) {
            wbt_list_init(&channel->subscriber_list->head);
            wbt_list_init(&channel->msg_list->head);
        } else {
            wbt_delete(channel->subscriber_list);
            wbt_delete(channel->msg_list);
            wbt_delete(channel);
            return NULL;
        }
        
        wbt_str_t channel_key;
        wbt_variable_to_str(channel->channel_id, channel_key);
        wbt_rbtree_node_t * channel_node = wbt_rbtree_insert(&wbt_mq_channels, &channel_key);
        if( channel_node == NULL ) {
            wbt_delete(channel->subscriber_list);
            wbt_delete(channel->msg_list);
            wbt_delete(channel);
            return NULL;
        }
        channel_node->value.ptr = channel;
        channel_node->value.len = sizeof(wbt_channel_t);
    }
    
    return channel;
}

wbt_channel_t * wbt_mq_channel_get(wbt_mq_id channel_id) {
    wbt_channel_t * channel;
    wbt_str_t channel_key;
    wbt_variable_to_str(channel_id, channel_key);
    wbt_rbtree_node_t * channel_node = wbt_rbtree_get(&wbt_mq_channels, &channel_key);

    if( channel_node == NULL ) {
        channel = wbt_mq_channel_create(channel_id);
    } else {
        channel = channel_node->value.ptr;
    }
    
    return channel;
}

void wbt_mq_channel_destory(wbt_channel_t *channel) {
    if( channel == NULL ) {
        return;
    }

    if( channel->subscriber_list ) {
        wbt_subscriber_list_t * pos;
        while(!wbt_list_empty(&channel->subscriber_list->head)) {
            pos = wbt_list_first_entry(&channel->subscriber_list->head, wbt_subscriber_list_t, head);
            wbt_list_del(&pos->head);
            wbt_delete(pos);
        }
        wbt_delete(channel->subscriber_list);
    }
    
    if( channel->msg_list ) {
        wbt_msg_list_t * pos;
        while(!wbt_list_empty(&channel->msg_list->head)) {
            pos = wbt_list_first_entry(&channel->msg_list->head, wbt_msg_list_t, head);
            wbt_list_del(&pos->head);
            wbt_delete(pos);
        }
        wbt_delete(channel->msg_list);
    }

    wbt_str_t channel_key;
    wbt_variable_to_str(channel->channel_id, channel_key);
    wbt_rbtree_node_t * channel_node = wbt_rbtree_get( &wbt_mq_channels, &channel_key );
    if( channel_node ) {
        wbt_rbtree_delete( &wbt_mq_channels, channel_node );
    }
}

wbt_status wbt_mq_channel_add_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber) {
    wbt_subscriber_list_t *subscriber_node = wbt_new(wbt_subscriber_list_t);
    if( subscriber_node == NULL ) {
        return WBT_ERROR;
    }
    subscriber_node->subscriber = subscriber;
    wbt_list_add(&subscriber_node->head, &channel->subscriber_list->head);
    
    return WBT_OK;
}

wbt_status wbt_mq_channel_del_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber) {
    // 从频道的 subscriber_list 中移除订阅者
    // TODO 这需要遍历链表，对于大量订阅者的频道来说不可接受。
    // 但是负载均衡模式又需要用到链表，这个地方还需要斟酌
    wbt_subscriber_list_t *subscriber_node;
    wbt_list_for_each_entry(subscriber_node, &channel->subscriber_list->head, head) {
        if( subscriber_node->subscriber == subscriber ) {
            wbt_list_del(&subscriber_node->head);
            wbt_delete(subscriber_node);
            return WBT_OK;
        }
    }
    return WBT_OK;
}