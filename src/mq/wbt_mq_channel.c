/* 
 * File:   wbt_mq_channel.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:32
 */

#include "wbt_mq_channel.h"
#include "wbt_mq_msg.h"

// 存储所有可用频道
static wbt_rbtree_t wbt_mq_channels;

wbt_status wbt_mq_channel_init() {
    wbt_rbtree_init(&wbt_mq_channels);

    return WBT_OK;
}

wbt_channel_t * wbt_mq_channel_create(wbt_mq_id channel_id) {
    wbt_channel_t * channel = wbt_calloc(sizeof(wbt_channel_t));
    
    if( channel ) {
        channel->channel_id = channel_id;
        channel->create = wbt_cur_mtime;

        channel->subscriber_list = wbt_calloc(sizeof(wbt_subscriber_list_t));
        channel->msg_list = wbt_mq_msg_create_node(0);
        if( channel->subscriber_list && channel->msg_list ) {
            wbt_list_init(&channel->subscriber_list->head);
            wbt_list_init(&channel->msg_list->head);
        } else {
            wbt_free(channel->subscriber_list);
            wbt_free(channel->msg_list);
            wbt_free(channel);
            return NULL;
        }
        
        wbt_str_t channel_key;
        wbt_variable_to_str(channel->channel_id, channel_key);
        wbt_rbtree_node_t * channel_node = wbt_rbtree_insert(&wbt_mq_channels, &channel_key);
        if( channel_node == NULL ) {
            wbt_free(channel->subscriber_list);
            wbt_free(channel->msg_list);
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
    wbt_rbtree_node_t * channel_node = wbt_rbtree_get(&wbt_mq_channels, &channel_key);

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

    if( channel->subscriber_list ) {
        wbt_subscriber_list_t * pos;
        while(!wbt_list_empty(&channel->subscriber_list->head)) {
            pos = wbt_list_first_entry(&channel->subscriber_list->head, wbt_subscriber_list_t, head);
            wbt_list_del(&pos->head);
            wbt_free(pos);
        }
        wbt_free(channel->subscriber_list);
    }
    
    if( channel->msg_list ) {
        wbt_msg_list_t * pos;
        while(!wbt_list_empty(&channel->msg_list->head)) {
            pos = wbt_list_first_entry(&channel->msg_list->head, wbt_msg_list_t, head);
            wbt_list_del(&pos->head);
            wbt_free(pos);
        }
        wbt_free(channel->msg_list);
    }

    wbt_str_t channel_key;
    wbt_variable_to_str(channel->channel_id, channel_key);
    wbt_rbtree_node_t * channel_node = wbt_rbtree_get( &wbt_mq_channels, &channel_key );
    if( channel_node ) {
        wbt_rbtree_delete( &wbt_mq_channels, channel_node );
    }
}

wbt_status wbt_mq_channel_add_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber) {
    wbt_subscriber_list_t *subscriber_node = wbt_calloc(sizeof(wbt_subscriber_list_t));
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
            wbt_free(subscriber_node);
            return WBT_OK;
        }
    }
    return WBT_OK;
}

extern wbt_rbtree_node_t *wbt_rbtree_node_nil;
void wbt_mq_print_channels_r(wbt_rbtree_node_t *node, wbt_str_t *resp, int maxlen) {
    void * tmp = wbt_malloc(1024);

    if(node != wbt_rbtree_node_nil) {
        wbt_mq_print_channels_r(node->left, resp, maxlen);
        
        wbt_channel_t *channel = (wbt_channel_t *)node->value.str;
        wbt_str_t channel_info;
        channel_info.str = tmp;
        channel_info.len = snprintf(tmp, 1024, "%016llX\n", channel->channel_id);
        if( channel_info.len > 1024 ) {
            channel_info.len = 1024;
        }
        wbt_strcat(resp, &channel_info, maxlen);
        
        wbt_mq_print_channels_r(node->right, resp, maxlen);
    }
    
    wbt_free(tmp);
}

void wbt_mq_print_channels(wbt_str_t *resp, int maxlen) {
    void * tmp = wbt_malloc(1024);
    wbt_str_t channel_count;
    channel_count.str = tmp;
    channel_count.len = snprintf(tmp, 1024, "%u channels\n", wbt_mq_channels.size);
    if( channel_count.len > 1024 ) {
        channel_count.len = 1024;
    }
    wbt_strcat(resp, &channel_count, maxlen);
    wbt_free(tmp);
    wbt_mq_print_channels_r(wbt_mq_channels.root, resp, maxlen);
}

void wbt_mq_print_channel(wbt_mq_id channel_id, wbt_str_t *resp, int maxlen) {
    wbt_channel_t *channel = wbt_mq_channel_get(channel_id);
    if(channel == NULL) {
        return;
    }
    
    void * tmp = wbt_malloc(1024);

    wbt_str_t channel_info;
    channel_info.str = tmp;

    channel_info.len = snprintf(tmp, 1024, "channel: %016llX\n", channel->channel_id);
    if( channel_info.len > 1024 ) {
        channel_info.len = 1024;
    }
    wbt_strcat(resp, &channel_info, maxlen);

    channel_info.len = snprintf(tmp, 1024, "\nsubscriber_list:\n");
    if( channel_info.len > 1024 ) {
        channel_info.len = 1024;
    }
    wbt_strcat(resp, &channel_info, maxlen);

    if( channel->subscriber_list ) {
        wbt_subscriber_t * subscriber;
        wbt_subscriber_list_t * subscriber_node;
        wbt_list_for_each_entry( subscriber_node, &channel->subscriber_list->head, head ) {
            subscriber = subscriber_node->subscriber;
            channel_info.len = snprintf(tmp, 1024, "%016llX\n", subscriber->subscriber_id);
            if( channel_info.len > 1024 ) {
                channel_info.len = 1024;
            }
            wbt_strcat(resp, &channel_info, maxlen);
        }
    }
    
    channel_info.len = snprintf(tmp, 1024, "\nmsg_list:\n");
    if( channel_info.len > 1024 ) {
        channel_info.len = 1024;
    }
    wbt_strcat(resp, &channel_info, maxlen);

    if( channel->msg_list ) {
        wbt_msg_t * msg;
        wbt_msg_list_t * msg_node;
        wbt_list_for_each_entry( msg_node, &channel->msg_list->head, head ) {
            msg = wbt_mq_msg_get(msg_node->msg_id);
            if(!msg || msg->expire <= wbt_cur_mtime) {
                channel_info.len = snprintf(tmp, 1024, "expired msg\n");
            } else {
                channel_info.len = snprintf(tmp, 1024, "%016llX %5u %5u %.*s\n",
                        msg->msg_id, msg->consumption_count, msg->delivery_count,
                        msg->data_len>100?100:(int)msg->data_len, (char *)msg->data);
            }
            if( channel_info.len > 1024 ) {
                channel_info.len = 1024;
            }
            wbt_strcat(resp, &channel_info, maxlen);
        }
    }
    
    wbt_free(tmp);
}