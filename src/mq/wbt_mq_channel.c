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
        if( channel->subscriber_list ) {
            wbt_list_init(&channel->subscriber_list->head);
        } else {
            wbt_mq_channel_destory(channel);
            return NULL;
        }

        if( wbt_heap_init( &channel->effective_heap, 128 ) != WBT_OK ) {
            wbt_mq_channel_destory(channel);
            return NULL;
        }
        
        wbt_str_t channel_key;
        wbt_variable_to_str(channel->channel_id, channel_key);
        wbt_rbtree_node_t * channel_node = wbt_rbtree_insert(&wbt_mq_channels, &channel_key);
        if( channel_node == NULL ) {
            wbt_mq_channel_destory(channel);
            return NULL;
        }
        channel_node->value.ptr = channel;
        channel_node->value.len = sizeof(channel);
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
    
    wbt_heap_destroy( &channel->effective_heap );

    wbt_str_t channel_key;
    wbt_variable_to_str(channel->channel_id, channel_key);
    wbt_rbtree_node_t * channel_node = wbt_rbtree_get( &wbt_mq_channels, &channel_key );
    if( channel_node ) {
        wbt_rbtree_delete( &wbt_mq_channels, channel_node );
    }
}