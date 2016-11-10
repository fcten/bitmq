/* 
 * File:   wbt_http_mq.c
 * Author: fcten
 *
 * Created on 2016年4月1日, 下午9:18
 */

#include "../common/wbt_string.h"
#include "../common/wbt_log.h"
#include "../mq/wbt_mq.h"
#include "../mq/wbt_mq_channel.h"
#include "../mq/wbt_mq_msg.h"
#include "../mq/wbt_mq_subscriber.h"
#include "../json/wbt_json.h"
#include "wbt_http.h"
#include "wbt_http_mq.h"

wbt_status wbt_http_mq_login(wbt_event_t *ev);
wbt_status wbt_http_mq_push(wbt_event_t *ev);
wbt_status wbt_http_mq_pull(wbt_event_t *ev);

wbt_status wbt_http_mq_send(wbt_event_t *ev, char *data, unsigned int len, int qos, int dup);
wbt_status wbt_http_mq_is_ready(wbt_event_t *ev);

wbt_status wbt_http_mq_status(wbt_event_t *ev);

wbt_status wbt_http_mq_status_general(wbt_event_t *ev);
wbt_status wbt_http_mq_status_message_general(wbt_event_t *ev);
wbt_status wbt_http_mq_status_channel_general(wbt_event_t *ev);
wbt_status wbt_http_mq_status_system_general(wbt_event_t *ev);
wbt_status  wbt_http_mq_status_subscriber_general(wbt_event_t *ev);
wbt_status wbt_http_mq_status_message(wbt_event_t *ev);
wbt_status wbt_http_mq_status_channel(wbt_event_t *ev);
wbt_status wbt_http_mq_status_system(wbt_event_t *ev);
wbt_status wbt_http_mq_status_subscriber(wbt_event_t *ev);

wbt_status wbt_http_mq_on_recv(wbt_event_t *ev);

wbt_module_t wbt_module_http1_mq = {
    wbt_string("http-mq"),
    NULL,
    NULL,
    NULL,
    wbt_http_mq_on_recv,
    NULL,
    NULL
};

wbt_status wbt_http_mq_on_recv(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_HTTP ) {
        return WBT_OK;
    }

    // 分发请求
    wbt_http_t * http = ev->data;

    // 只过滤 404 响应
    if( http->status != STATUS_404 ) {
        return WBT_OK;
    }

    wbt_str_t login  = wbt_string("/mq/login/");
    wbt_str_t pull   = wbt_string("/mq/pull/");
    wbt_str_t push   = wbt_string("/mq/push/");
    wbt_str_t status = wbt_string("/mq/status/");
    
    wbt_str_t http_uri;
    wbt_offset_to_str(http->uri, http_uri, ev->buff);
    
    if( wbt_strcmp( &http_uri, &login ) == 0 ) {
        return wbt_http_mq_login(ev);
    } else if( wbt_strcmp( &http_uri, &pull ) == 0 ) {
        return wbt_http_mq_pull(ev);
    } else if( wbt_strcmp( &http_uri, &push ) == 0 ) {
        return wbt_http_mq_push(ev);
    } else if( wbt_strncmp( &http_uri, &status, status.len ) == 0 ) {
        return wbt_http_mq_status(ev);
    }
    return WBT_OK;
}

wbt_status wbt_http_mq_login(wbt_event_t *ev) {
    // 解析请求
    wbt_http_t * http = ev->data;

    // 必须是 POST 请求
    if( http->method != METHOD_POST ) {
        http->status = STATUS_405;
        return WBT_OK;
    }
    
    // 处理 body
    if( http->body.len <= 0 ) {
        http->status = STATUS_403;
        return WBT_OK;
    }
    
    if( wbt_mq_login(ev) != WBT_OK ) {
        http->status = STATUS_403;
        return WBT_OK;
    }
    
    wbt_mq_set_send_cb(ev, wbt_http_mq_send);
    wbt_mq_set_is_ready_cb(ev, wbt_http_mq_is_ready);

    // 在所有想要订阅的频道的 subscriber_list 中添加该订阅者
    wbt_str_t channel_ids;
    wbt_offset_to_str(http->body, channel_ids, ev->buff);
    wbt_mq_id channel_id = wbt_str_to_ull(&channel_ids, 10);

    if( wbt_mq_subscribe(ev, channel_id) != WBT_OK ) {
        http->status = STATUS_503;
        return WBT_OK;
    }

    http->status = STATUS_200;  
    return WBT_OK;
}

wbt_status wbt_http_mq_push(wbt_event_t *ev) {
    // 解析请求
    wbt_http_t * http = ev->data;

    // 必须是 POST 请求
    if( http->method != METHOD_POST ) {
        http->status = STATUS_405;
        return WBT_OK;
    }
    
    // 解析请求
    wbt_str_t data;
    wbt_offset_to_str(http->body, data, ev->buff);

    if( wbt_mq_push(ev, data.str, data.len) != WBT_OK ) {
        // TODO 需要返回更详细的错误原因
        http->status = STATUS_403;
        return WBT_OK;
    }

    // TODO 返回 msg_id
    http->status = STATUS_200;
    return WBT_OK;
}

wbt_status wbt_http_mq_pull_timeout(wbt_timer_t *timer) {
    wbt_event_t *ev = wbt_timer_entry(timer, wbt_event_t, timer);

    // 固定返回一个空的响应，通知客户端重新发起 pull 请求
    wbt_http_t * http = ev->data;
    
    http->state = STATE_SENDING;
    http->status = STATUS_204;

    if( wbt_http_process(ev) != WBT_OK ) {
        wbt_on_close(ev);
    } else {
        /* 等待socket可写 */
        ev->timer.on_timeout = wbt_conn_close;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.event_timeout;
        ev->on_send = wbt_on_send;
        ev->events = WBT_EV_WRITE | WBT_EV_ET;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_http_mq_is_ready(wbt_event_t *ev) {
    if( ev->timer.on_timeout == wbt_http_mq_pull_timeout ) {
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

wbt_status wbt_http_mq_pull(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;

    wbt_msg_t *msg;
    if( wbt_mq_pull(ev, &msg) != WBT_OK ) {
        http->status = STATUS_403;
        return WBT_OK;
    }
    
    if( msg == NULL ) {
        if(1) {
            // 如果没有可发送的消息，挂起请求
            http->state = STATE_BLOCKING;

            ev->timer.timeout = wbt_cur_mtime + 30000;
            ev->timer.on_timeout = wbt_http_mq_pull_timeout;

            if(wbt_event_mod(ev) != WBT_OK) {
                return WBT_ERROR;
            }

            return WBT_OK;
        } else {
            // 如果没有可发送的消息，直接返回
            return wbt_http_mq_pull_timeout(&ev->timer);
        }
    } else {
        json_object_t *obj = wbt_mq_msg_print(msg);

        http->resp_body_memory.len = 1024 * 64;
        http->resp_body_memory.str = wbt_malloc( http->resp_body_memory.len );
        if( http->resp_body_memory.str == NULL ) {
            http->status = STATUS_503;
            return WBT_OK;
        }
        char *p = http->resp_body_memory.str;
        size_t l = http->resp_body_memory.len;
        json_print(obj, &p, &l);
        http->resp_body_memory.len -= l;
        http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );
        
        json_delete_object(obj);
        
        http->status = STATUS_200;

        return WBT_OK;
    }

    return WBT_OK;
}

wbt_status wbt_http_mq_send(wbt_event_t *ev, char *data, unsigned int len, int qos, int dup) {
    wbt_http_t * http = ev->data;

    http->resp_body_memory.len = len;
    http->resp_body_memory.str = wbt_strdup(data, len);
    http->status = STATUS_200;
    http->state = STATE_SENDING;

    if( wbt_http_process(ev) != WBT_OK ) {
        // 这个方法被调用时，可能正在遍历订阅者链表，这时不能关闭连接
        // TODO 以合适的方法关闭连接
        return WBT_ERROR;
    }

    /* 等待socket可写 */
    ev->timer.on_timeout = wbt_conn_close;
    ev->timer.timeout    = wbt_cur_mtime + wbt_conf.event_timeout;
    ev->on_send = wbt_on_send;
    ev->events  = WBT_EV_WRITE | WBT_EV_ET;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_http_mq_status(wbt_event_t *ev) {
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
    wbt_str_t status_subscriber = wbt_string("/mq/status/subscriber/");
    
    wbt_str_t http_uri;
    wbt_offset_to_str(http->uri, http_uri, ev->buff);
    
    if( wbt_strcmp( &http_uri, &status ) == 0 ) {
        return wbt_http_mq_status_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_msg ) == 0 ) {
        return wbt_http_mq_status_message_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_channel ) == 0 ) {
        return wbt_http_mq_status_channel_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_system ) == 0 ) {
        return wbt_http_mq_status_system_general(ev);
    } else if( wbt_strcmp( &http_uri, &status_subscriber ) == 0 ) {
        return wbt_http_mq_status_subscriber_general(ev);
    } else if( wbt_strncmp( &http_uri, &status_msg, status_msg.len ) == 0 ) {
        return wbt_http_mq_status_message(ev);
    } else if( wbt_strncmp( &http_uri, &status_channel, status_channel.len ) == 0 ) {
        return wbt_http_mq_status_channel(ev);
    } else if( wbt_strncmp( &http_uri, &status_channel, status_channel.len ) == 0 ) {
        return wbt_http_mq_status_system(ev);
    } else if( wbt_strncmp( &http_uri, &status_subscriber, status_subscriber.len ) == 0 ) {
        return wbt_http_mq_status_subscriber(ev);
    }

    return WBT_OK;
}

wbt_status wbt_http_mq_status_general(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;
    
    http->resp_body_memory.len = 10240;
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
    long long int system_memory = wbt_mem_usage();
    
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
    json_append(system, wbt_mq_str_memory.str, wbt_mq_str_memory.len, JSON_LONGLONG, &system_memory, 0);

    char *ptr = http->resp_body_memory.str;
    size_t len = http->resp_body_memory.len;
    json_print(obj, &ptr, &len);
    http->resp_body_memory.len = http->resp_body_memory.len - len;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );

    json_delete_object(obj);
    
    http->status = STATUS_200;

    return WBT_OK;
}

wbt_status wbt_http_mq_status_message_general(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_http_mq_status_channel_general(wbt_event_t *ev) {
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

wbt_status wbt_http_mq_status_system_general(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_http_mq_status_message(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;
    
    wbt_str_t http_uri;
    wbt_offset_to_str(http->uri, http_uri, ev->buff);
    
    wbt_str_t msg_id_str;
    msg_id_str.str = http_uri.str + 19;
    msg_id_str.len = http_uri.len - 19;
    wbt_mq_id msg_id = wbt_str_to_ull(&msg_id_str, 10);
    
    wbt_msg_t *msg = wbt_mq_msg_get(msg_id);
    if(msg == NULL) {
        http->status = STATUS_404;
        return WBT_OK;
    }
    
    http->resp_body_memory.len = 1024 * 64;
    http->resp_body_memory.str = wbt_malloc(http->resp_body_memory.len);
    if( http->resp_body_memory.str == NULL ) {
        http->status = STATUS_503;
        return WBT_OK;
    }
    
    json_object_t *obj = wbt_mq_msg_print(msg);

    char *p = http->resp_body_memory.str;
    size_t l = 1024 * 64;
    json_print(obj, &p, &l);
    http->resp_body_memory.len = 1024 * 64 - l;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );    

    json_delete_object(obj);
    
    http->status = STATUS_200;
    
    return WBT_OK;
}

wbt_status wbt_http_mq_status_channel(wbt_event_t *ev) {
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
    json_append(subscriber, wbt_mq_str_list.str,  wbt_mq_str_list.len,  JSON_ARRAY,    subscriber_list,           0);

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

wbt_status wbt_http_mq_status_system(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status  wbt_http_mq_status_subscriber_general(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;
    
    http->resp_body_memory.len = 1024;
    http->resp_body_memory.str = wbt_malloc( http->resp_body_memory.len );
    if( http->resp_body_memory.str == NULL ) {
        http->status = STATUS_503;
        return WBT_OK;
    }

    long long int subscriber_active = wbt_mq_subscriber_status_active();
    
    json_object_t * obj             = json_create_object();
    json_object_t * subscriber_list = json_create_array();
    
    json_append(obj, wbt_mq_str_active.str, wbt_mq_str_active.len, JSON_LONGLONG, &subscriber_active, 0);
    json_append(obj, wbt_mq_str_list.str,   wbt_mq_str_list.len,   JSON_ARRAY,    subscriber_list,    0);
    
    wbt_mq_subscriber_print_all(subscriber_list);
    
    char *ptr = http->resp_body_memory.str;
    size_t len = http->resp_body_memory.len;
    json_print(obj, &ptr, &len);
    http->resp_body_memory.len = http->resp_body_memory.len - len;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );

    json_delete_object(obj);
    
    http->status = STATUS_200;

    return WBT_OK;
}

wbt_status wbt_http_mq_status_subscriber(wbt_event_t *ev) {
    wbt_http_t * http = ev->data;
    
    wbt_str_t http_uri;
    wbt_offset_to_str(http->uri, http_uri, ev->buff);
    
    wbt_str_t subscriber_id_str;
    subscriber_id_str.str = http_uri.str + 22;
    subscriber_id_str.len = http_uri.len - 22;
    wbt_mq_id subscriber_id = wbt_str_to_ull(&subscriber_id_str, 10);

    wbt_subscriber_t *subscriber = wbt_mq_subscriber_get(subscriber_id);
    if(subscriber == NULL) {
        http->status = STATUS_404;
        return WBT_OK;
    }

    http->resp_body_memory.len = 1024;
    http->resp_body_memory.str = wbt_malloc(http->resp_body_memory.len);
    if( http->resp_body_memory.str == NULL ) {
        http->status = STATUS_503;
        return WBT_OK;
    }

    json_object_t * obj = wbt_mq_subscriber_print(subscriber);
    json_object_t * message_list = json_create_array();
    json_object_t * channel_list = json_create_array();
    
    json_append(obj, wbt_mq_str_message.str, wbt_mq_str_message.len, JSON_ARRAY, message_list, 0);
    json_append(obj, wbt_mq_str_channel.str, wbt_mq_str_channel.len, JSON_ARRAY, channel_list, 0);
    
    wbt_mq_subscriber_msg_print(subscriber, message_list);
    wbt_mq_subscriber_channel_print(subscriber, channel_list);

    struct sockaddr_in sa;
    int sa_len = sizeof( sa );
    if( !getpeername( subscriber->ev->fd, ( struct sockaddr * )&sa, &sa_len ) ) {
        wbt_str_t ip;
        ip.str = inet_ntoa( sa.sin_addr );
        ip.len = strlen( ip.str );
        json_append( obj, wbt_mq_str_ip.str, wbt_mq_str_ip.len, JSON_STRING, ip.str, ip.len );
    }

    char *ptr = http->resp_body_memory.str;
    size_t len = http->resp_body_memory.len;
    json_print(obj, &ptr, &len);
    http->resp_body_memory.len = http->resp_body_memory.len - len;
    http->resp_body_memory.str = wbt_realloc( http->resp_body_memory.str, http->resp_body_memory.len );

    json_delete_object(obj);

    http->status = STATUS_200;

    return WBT_OK;
}