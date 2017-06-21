/* 
 * File:   wbt_mq_msg.c
 * Author: fcten
 *
 * Created on 2016年1月18日, 下午4:08
 */

#include "wbt_mq_msg.h"
#include "wbt_mq_channel.h"

extern wbt_atomic_t wbt_wating_to_exit;

// 存储所有已接收到的消息
static wbt_rb_t wbt_mq_messages;

wbt_status wbt_mq_msg_init() {
    wbt_rb_init(&wbt_mq_messages, WBT_RB_KEY_LONGLONG);

    return WBT_OK;
}

static wbt_mq_id wbt_msg_create_count = 0;
static wbt_mq_id wbt_msg_effect_count = 0;
static wbt_mq_id wbt_msg_delete_count = 0;

void wbt_mq_msg_update_create_count(wbt_mq_id msg_id) {
    if( wbt_msg_create_count < msg_id ) {
        wbt_msg_create_count = msg_id;
    }
}

wbt_msg_t * wbt_mq_msg_create(wbt_mq_id msg_id) {
    wbt_msg_t * msg = wbt_calloc(sizeof(wbt_msg_t));
    
    if( msg ) {
        if( msg_id ) {
            msg->msg_id = msg_id;
            if( wbt_msg_create_count < msg_id ) {
                wbt_msg_create_count = msg_id;
            }
        } else {
            msg->msg_id = ++wbt_msg_create_count;
        }
        msg->seq_id = 0;
        msg->create = wbt_cur_mtime;
        msg->state = MSG_CREATED;
        
        wbt_str_t msg_key;
        wbt_variable_to_str(msg->msg_id, msg_key);
        wbt_rb_node_t * msg_node = wbt_rb_insert(&wbt_mq_messages, &msg_key);
        if( msg_node == NULL ) {
            wbt_log_debug("msg_id conflict: %llu\n", msg_id);
            wbt_free(msg);
            return NULL;
        }
        msg_node->value.str = (char *)msg;
        
        wbt_log_debug("msg %lld create\n", msg->msg_id);
    }
    
    return msg;
}

wbt_msg_t * wbt_mq_msg_get(wbt_mq_id msg_id) {
    wbt_str_t msg_key;
    wbt_variable_to_str(msg_id, msg_key);
    wbt_rb_node_t * msg_node = wbt_rb_get(&wbt_mq_messages, &msg_key);
    if( msg_node == NULL ) {
        return NULL;
    }
    return (wbt_msg_t *)msg_node->value.str;
}

void wbt_mq_msg_destory(wbt_msg_t *msg) {
    if( msg == NULL ) {
        return;
    }
    
    wbt_log_debug("msg %lld deleted\n", msg->msg_id);
    
    // 从消息所对应的频道中删除该消息
    // TODO 这里没有必要再做一次 wbt_mq_channel_get 查询
    if( msg->type != MSG_ACK ) {
        wbt_channel_t * channel = wbt_mq_channel_get(msg->consumer_id);
        if( channel ) {
            wbt_mq_channel_del_msg(channel, msg);
        }
    }
    
    wbt_timer_del(&msg->timer);
    
    wbt_free(msg->data);

    wbt_str_t msg_key;
    wbt_variable_to_str(msg->msg_id, msg_key);
    wbt_rb_node_t * msg_node = wbt_rb_get( &wbt_mq_messages, &msg_key );
    if( msg_node ) {
        wbt_rb_delete( &wbt_mq_messages, msg_node );
    }
    
    wbt_msg_delete_count ++;
}

wbt_status wbt_mq_msg_event_expire(wbt_timer_t *timer) {
    // Bugfix: BitMQ 在退出时会执行并删除所有尚未超时的定时事件，本定时事件不能在程
    // 序退出时执行。
    if( wbt_wating_to_exit ) {
        return WBT_OK;
    }

    wbt_msg_t *msg = wbt_timer_entry(timer, wbt_msg_t, timer);

    wbt_mq_msg_destory(msg);
    
    return WBT_OK;
}

wbt_status wbt_mq_msg_event_effect(wbt_timer_t *timer) {
    // Bugfix: BitMQ 在退出时会执行并删除所有尚未超时的定时事件，本定时事件不能在程
    // 序退出时执行。
    if( wbt_wating_to_exit ) {
        return WBT_OK;
    }

    wbt_msg_t *msg = wbt_timer_entry(timer, wbt_msg_t, timer);
    
    msg->timer.on_timeout = wbt_mq_msg_event_expire;
    msg->timer.timeout    = msg->expire;
    wbt_timer_add(&msg->timer);
    
    if( msg->state == MSG_CREATED ) {
        // !!TODO: 同一时间的定时事件无法保证先后执行的顺序，这回导致同一时间的消息 ID 乱序
        msg->seq_id = ++wbt_msg_effect_count;
        msg->state = MSG_EFFECTIVE;
        
        wbt_channel_t * channel = wbt_mq_channel_get(msg->consumer_id);
        if( channel == NULL ) {
            return WBT_ERROR;
        }

        // 将该消息投递给对应频道
        if( wbt_mq_channel_add_msg(channel, msg) != WBT_OK ) {
            return WBT_ERROR;
        }
        
        wbt_log_debug("msg_id %lld: effected\n", msg->msg_id);
    }
    
    return WBT_OK;
}

wbt_status wbt_mq_msg_timer_add(wbt_msg_t *msg) {
    wbt_timer_del(&msg->timer);
    
    if( msg->expire < wbt_cur_mtime ) {
        // 过期消息
        msg->timer.on_timeout = wbt_mq_msg_event_expire;
        msg->timer.timeout    = msg->expire;
        return wbt_timer_add(&msg->timer);
    } else if( msg->effect > wbt_cur_mtime ) {
        // 未生效消息
        msg->timer.on_timeout = wbt_mq_msg_event_effect;
        msg->timer.timeout    = msg->effect;
        return wbt_timer_add(&msg->timer);
    } else {
        // 已生效消息
        msg->timer.on_timeout = wbt_mq_msg_event_effect;
        msg->timer.timeout    = msg->effect;
        return wbt_timer_add(&msg->timer);
    }
}

wbt_status wbt_mq_msg_refer_inc(wbt_msg_t *msg) {
#ifdef WBT_DEBUG
    if( msg->effect > wbt_cur_mtime ) {
        wbt_log_debug("ERROR: msg %lld is not yet effective", msg->msg_id);
        return WBT_ERROR;
    }
#endif

    msg->reference_count ++;
    
    wbt_timer_del(&msg->timer);
    
    return WBT_OK;
}

wbt_status wbt_mq_msg_refer_dec(wbt_msg_t *msg) {
#ifdef WBT_DEBUG
    if(msg->reference_count == 0) {
        wbt_log_debug("ERROR: too many decrease to message reference");
        return WBT_ERROR;
    }
#endif

    msg->reference_count --;
    
    if( msg->reference_count == 0 ) {
        wbt_mq_msg_timer_add(msg);
    }
    
    return WBT_OK;
}

wbt_msg_list_t * wbt_mq_msg_create_node(wbt_msg_t *msg) {
    wbt_msg_list_t * msg_node = wbt_calloc(sizeof(wbt_msg_list_t));
    if( msg_node == NULL ) {
        return NULL;
    }
    msg_node->msg_id = msg->msg_id;

    return msg_node;
}

void wbt_mq_msg_destory_node(wbt_msg_list_t *node) {
    wbt_free(node);
}

long long int wbt_mq_msg_status_total() {
    return (long long int)wbt_msg_create_count;
}

long long int wbt_mq_msg_status_active() {
    return (long long int)wbt_mq_messages.size;
}

long long int wbt_mq_msg_status_delayed() {
    return 0;
}

long long int wbt_mq_msg_status_waiting_ack() {
    return 0;
}

json_object_t* wbt_mq_msg_print(wbt_msg_t *msg) {
    json_object_t * obj = json_create_object();
    
    json_append(obj, wbt_mq_str_msg_id.str,       wbt_mq_str_msg_id.len,       JSON_LONGLONG, &msg->msg_id,      0);
    if( msg->producer_id ) {
        json_append(obj, wbt_mq_str_producer_id.str, wbt_mq_str_producer_id.len, JSON_LONGLONG, &msg->producer_id, 0);
    }
    json_append(obj, wbt_mq_str_consumer_id.str,  wbt_mq_str_consumer_id.len, JSON_LONGLONG, &msg->consumer_id, 0 );
    json_append(obj, wbt_mq_str_create.str,       wbt_mq_str_create.len,      JSON_LONGLONG, &msg->create,      0);

    json_task_t t;
    t.str = msg->data;
    t.len = msg->data_len;
    t.callback = NULL;

    if( json_parser(&t) != 0 ) {
        json_delete_object(t.root);

        json_append(obj, wbt_mq_str_data.str, wbt_mq_str_data.len, JSON_STRING,         msg->data, msg->data_len);
    } else {
        json_append(obj, wbt_mq_str_data.str, wbt_mq_str_data.len, t.root->object_type, t.root   , 0);
    }
    
    return obj;
}

wbt_rb_t* wbt_mq_msg_get_all() {
    return &wbt_mq_messages;
}
