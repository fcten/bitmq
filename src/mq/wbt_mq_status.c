/* 
 * File:   wbt_mq_status.c
 * Author: fcten
 *
 * Created on 2016年2月14日, 下午4:00
 */

#include "wbt_mq_status.h"
#include "wbt_mq_channel.h"
#include "wbt_mq_msg.h"
#include "wbt_mq_subscriber.h"
#include "../json/wbt_json.h"

wbt_status wbt_mq_status(wbt_event_t *ev) {
    // 分发请求
    wbt_http_t * http = ev->data;

    // 只过滤 404 响应
    if( http->status != STATUS_404 ) {
        return WBT_OK;
    }

    // 必须是 GET 请求
    if( http->method != METHOD_GET ) {
        http->status = STATUS_405;
        return WBT_OK;
    }
    
    // TODO 实际的访问控制需要自己通过 IP 实现
    wbt_str_t access_control_allow_origin = wbt_string("*");
    wbt_http_set_header( http, HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, &access_control_allow_origin );

    wbt_str_t status = wbt_string("/mq/status/");
    wbt_str_t status_system = wbt_string("/mq/status/system/");
    wbt_str_t status_msg = wbt_string("/mq/status/message/");
    wbt_str_t status_channel = wbt_string("/mq/status/channel/");
    
    wbt_str_t http_uri;
    wbt_offset_to_str(http->uri, http_uri, ev->buff);
    
    if( wbt_strcmp( &http_uri, &status ) == 0 ) {
        return wbt_mq_status_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_msg ) == 0 ) {
        return wbt_mq_status_message_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_channel ) == 0 ) {
        return wbt_mq_status_channel_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_system ) == 0 ) {
        return wbt_mq_status_system_general(ev);
    } else if( wbt_strncmp( &http_uri, &status_msg, status_msg.len ) == 0 ) {
        return wbt_mq_status_message(ev);
    } else if( wbt_strncmp( &http_uri, &status_channel, status_channel.len ) == 0 ) {
        return wbt_mq_status_channel(ev);
    } else if( wbt_strncmp( &http_uri, &status_channel, status_channel.len ) == 0 ) {
        return wbt_mq_status_system(ev);
    }

    return WBT_OK;
}

wbt_status wbt_mq_status_general(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;
    
    http->resp_body_memory.len = 1024;
    http->resp_body_memory.str = wbt_malloc( http->resp_body_memory.len );
    if( http->resp_body_memory.str == NULL ) {
        http->status = STATUS_503;
        return WBT_OK;
    }

    long long int message_total = wbt_mq_msg_status_total();
    long long int message_active = wbt_mq_msg_status_active();
    long long int message_delayed = wbt_mq_msg_status_delayed();
    long long int message_waiting_ack = wbt_mq_msg_status_waiting_ack();
    long long int channel_active = wbt_mq_channel_status_active();
    long long int subscriber_active = wbt_mq_subscriber_status_active();
    long long int system_uptime = wbt_mq_uptime();
    
    json_object_t * obj        = json_create_object();
    json_object_t * message    = json_create_object();
    json_object_t * channel    = json_create_object();
    json_object_t * subscriber = json_create_object();
    json_object_t * system     = json_create_object();
    
    json_append(obj, wbt_mq_str_message.str,    wbt_mq_str_message.len,    JSON_OBJECT, message,    0);
    json_append(obj, wbt_mq_str_channel.str,    wbt_mq_str_channel.len,    JSON_OBJECT, channel,    0);
    json_append(obj, wbt_mq_str_subscriber.str, wbt_mq_str_subscriber.len, JSON_OBJECT, subscriber, 0);
    json_append(obj, wbt_mq_str_system.str,     wbt_mq_str_system.len,     JSON_OBJECT, system,     0);

    json_append(message, wbt_mq_str_total.str,       wbt_mq_str_total.len,       JSON_LONGLONG, &message_total,       0);
    json_append(message, wbt_mq_str_active.str,      wbt_mq_str_active.len,      JSON_LONGLONG, &message_active,      0);
    json_append(message, wbt_mq_str_delayed.str,     wbt_mq_str_delayed.len,     JSON_LONGLONG, &message_delayed,     0);
    json_append(message, wbt_mq_str_waiting_ack.str, wbt_mq_str_waiting_ack.len, JSON_LONGLONG, &message_waiting_ack, 0);

    json_append(channel, wbt_mq_str_active.str, wbt_mq_str_active.len, JSON_LONGLONG, &channel_active, 0);

    json_append(subscriber, wbt_mq_str_active.str, wbt_mq_str_active.len, JSON_LONGLONG, &subscriber_active, 0);
    
    json_append(system, wbt_mq_str_uptime.str, wbt_mq_str_uptime.len, JSON_LONGLONG, &system_uptime, 0);

    char *ptr = http->resp_body_memory.str;
    size_t len = http->resp_body_memory.len;
    json_print(obj, &ptr, &len);
    http->resp_body_memory.len = http->resp_body_memory.len - len;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );

    json_delete_object(obj);
    
    http->status = STATUS_200;

    return WBT_OK;
}

wbt_status wbt_mq_status_message_general(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_mq_status_channel_general(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;

    http->resp_body_memory.len = 4096;
    http->resp_body_memory.str = wbt_malloc(http->resp_body_memory.len);
    if( http->resp_body_memory.str == NULL ) {
        http->status = STATUS_503;
        return WBT_OK;
    }
    
    long long int channel_active = wbt_mq_channel_status_active();
    
    json_object_t * obj        = json_create_object();
    json_object_t * list       = json_create_array();
    json_append(obj, wbt_mq_str_list.str,  wbt_mq_str_list.len,  JSON_ARRAY,    list,            0);
    json_append(obj, wbt_mq_str_total.str, wbt_mq_str_total.len, JSON_LONGLONG, &channel_active, 0);

    wbt_mq_channel_print_all(list);
    
    char *ptr = http->resp_body_memory.str;
    size_t len = http->resp_body_memory.len;
    json_print(obj, &ptr, &len);
    http->resp_body_memory.len = http->resp_body_memory.len - len;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );

    json_delete_object(obj);
    
    http->status = STATUS_200;

    return WBT_OK;
}

wbt_status wbt_mq_status_system_general(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_mq_status_message(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_mq_status_channel(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;
    
    wbt_str_t http_uri;
    wbt_offset_to_str(http->uri, http_uri, ev->buff);
    
    wbt_str_t channel_id_str;
    channel_id_str.str = http_uri.str + 19;
    channel_id_str.len = http_uri.len - 19;
    wbt_mq_id channel_id = wbt_str_to_ull(&channel_id_str, 10);

    wbt_channel_t *channel = wbt_mq_channel_get(channel_id);
    if(channel == NULL) {
        http->status = STATUS_404;
        return WBT_OK;
    }

    http->resp_body_memory.len = 1024;
    http->resp_body_memory.str = wbt_malloc(http->resp_body_memory.len);
    if( http->resp_body_memory.str == NULL ) {
        http->status = STATUS_503;
        return WBT_OK;
    }

    json_object_t * obj             = json_create_object();
    json_object_t * message         = json_create_object();
    json_object_t * subscriber      = json_create_object();
    json_object_t * message_list    = json_create_array();
    json_object_t * subscriber_list = json_create_array();
    
    json_append(obj, wbt_mq_str_channel_id.str, wbt_mq_str_channel_id.len, JSON_LONGLONG, &channel->channel_id, 0);
    json_append(obj, wbt_mq_str_message.str,    wbt_mq_str_message.len,    JSON_OBJECT,   message,              0);
    json_append(obj, wbt_mq_str_subscriber.str, wbt_mq_str_subscriber.len, JSON_OBJECT,   subscriber,           0);

    json_append(message, wbt_mq_str_total.str, wbt_mq_str_total.len, JSON_INTEGER, &channel->queue.size, 0);
    json_append(message, wbt_mq_str_list.str,  wbt_mq_str_list.len,  JSON_ARRAY,    message_list,        0);

    json_append(subscriber, wbt_mq_str_total.str, wbt_mq_str_total.len, JSON_INTEGER, &channel->subscriber_count, 0);
    json_append(subscriber, wbt_mq_str_list.str,  wbt_mq_str_list.len,  JSON_ARRAY,    subscriber_list,            0);

    wbt_mq_channel_msg_print(channel, message_list);
    wbt_mq_channel_subscriber_print(channel, subscriber_list);
    
    char *ptr = http->resp_body_memory.str;
    size_t len = http->resp_body_memory.len;
    json_print(obj, &ptr, &len);
    http->resp_body_memory.len = http->resp_body_memory.len - len;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );

    json_delete_object(obj);

    http->status = STATUS_200;

    return WBT_OK;
}

wbt_status wbt_mq_status_system(wbt_event_t *ev) {
    return WBT_OK;
}