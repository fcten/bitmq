/* 
 * File:   wbt_mq.c
 * Author: fcten
 *
 * Created on 2016年1月14日, 上午10:59
 */

#include "wbt_mq.h"
#include "wbt_mq_channel.h"
#include "wbt_mq_msg.h"
#include "wbt_mq_subscriber.h"
#include "wbt_mq_persistence.h"
#include "wbt_mq_auth.h"
#include "../json/wbt_json.h"

wbt_str_t wbt_mq_str_message       = wbt_string("message");
wbt_str_t wbt_mq_str_channel       = wbt_string("channel");
wbt_str_t wbt_mq_str_subscriber    = wbt_string("subscriber");

wbt_str_t wbt_mq_str_total         = wbt_string("total");
wbt_str_t wbt_mq_str_active        = wbt_string("active");
wbt_str_t wbt_mq_str_delayed       = wbt_string("delayed");
wbt_str_t wbt_mq_str_waiting_ack   = wbt_string("waiting_ack");

wbt_str_t wbt_mq_str_system        = wbt_string("system");
wbt_str_t wbt_mq_str_uptime        = wbt_string("uptime");
wbt_str_t wbt_mq_str_memory        = wbt_string("memory");

wbt_str_t wbt_mq_str_list          = wbt_string("list");

wbt_str_t wbt_mq_str_msg_id        = wbt_string("msg_id");
wbt_str_t wbt_mq_str_channel_id    = wbt_string("channel_id");
wbt_str_t wbt_mq_str_subscriber_id = wbt_string("subscriber_id");
wbt_str_t wbt_mq_str_producer_id   = wbt_string("producer_id");
wbt_str_t wbt_mq_str_consumer_id   = wbt_string("consumer_id");
wbt_str_t wbt_mq_str_create        = wbt_string("create");
wbt_str_t wbt_mq_str_effect        = wbt_string("effect");
wbt_str_t wbt_mq_str_expire        = wbt_string("expire");
wbt_str_t wbt_mq_str_delivery_mode = wbt_string("delivery_mode");
wbt_str_t wbt_mq_str_data          = wbt_string("data");

wbt_str_t wbt_mq_str_ip            = wbt_string( "ip" );

wbt_module_t wbt_module_mq = {
    wbt_string("mq"),
    wbt_mq_init, // init
    NULL, // exit
    NULL, // on_conn
    NULL, //wbt_mq_on_recv
    NULL, // on_send
    wbt_mq_on_close,
    NULL
};

wbt_status wbt_mq_init() {
    if( wbt_mq_auth_init() != WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_mq_channel_init() != WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_mq_subscriber_init() != WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_mq_msg_init() != WBT_OK ) {
        return WBT_ERROR;
    }
    
    if( wbt_mq_persist_init() != WBT_OK ) {
        return WBT_ERROR;
    }
    
    wbt_mq_uptime();

    return WBT_OK;
}

time_t wbt_mq_uptime() {
    static time_t start_time = 0;

    if(!start_time) {
        start_time = wbt_cur_mtime;
    }
    
    return (wbt_cur_mtime - start_time)/1000;
}

wbt_status wbt_mq_on_close(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_OK;
    }

    // 遍历所有订阅的频道
    wbt_channel_list_t *channel_node;
    wbt_list_for_each_entry(channel_node, wbt_channel_list_t, &subscriber->channel_list.head, head) {
        // 从该频道的 subscriber_list 中移除该订阅者
        wbt_mq_channel_del_subscriber(channel_node->channel, subscriber);
    }

    // 删除该订阅者
    wbt_mq_subscriber_destory(subscriber);

    ev->ctx = NULL;
    
    return WBT_OK;
}

wbt_status wbt_mq_login(wbt_event_t *ev) {
    // 检查是否已经登录过
    if( ev->ctx != NULL ) {
        wbt_mq_on_close(ev);
    }

    // 创建一个新的订阅者并初始化
    wbt_subscriber_t * subscriber = wbt_mq_subscriber_create();
    if( subscriber == NULL ) {
        // TODO 登录失败
        return WBT_ERROR;
    }
    
    subscriber->ev = ev;
    ev->ctx = subscriber;
    
    wbt_log_debug("new subscriber %lld fron conn %d\n", subscriber->subscriber_id, ev->fd);

    return WBT_OK;
}

wbt_status wbt_mq_subscribe(wbt_event_t *ev, wbt_mq_id channel_id) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }

    wbt_channel_t * channel = wbt_mq_channel_get(channel_id);
    if( channel == NULL ) {
        // TODO 服务器繁忙
        return WBT_ERROR;
    }
    
    // 检查是否已经订阅过
    if( wbt_mq_subscriber_has_channel( subscriber, channel ) ) {
        // TODO 重复订阅
        return WBT_ERROR;
    }

    // 在该订阅者中添加一个频道 & 在该频道中添加一个订阅者
    if( wbt_mq_subscriber_add_channel(subscriber, channel) != WBT_OK ||
        wbt_mq_channel_add_subscriber(channel, subscriber) != WBT_OK ) {
        // TODO 操作可能需要撤回
        // TODO 服务器繁忙
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

// TODO 使用全局变量不是多线程安全的，如果未来 BitMQ 引入了多线程，这里可能带来问题
wbt_msg_t wbt_mq_parsed_msg;

void wbt_mq_json_parser_cb(json_task_t *task, json_object_t *node) {
    if( node->next != NULL ) {
        return;
    }

    wbt_str_t key;
    key.str = node->key;
    key.len = node->key_len;

    switch( node->value_type ) {
        case JSON_LONGLONG:
            if ( wbt_strcmp(&key, &wbt_mq_str_consumer_id) == 0  ) {
                wbt_mq_parsed_msg.consumer_id = node->value.l;
            } else if ( wbt_strcmp(&key, &wbt_mq_str_producer_id) == 0 ) {
                wbt_mq_parsed_msg.producer_id = node->value.l;
            } else if ( wbt_strcmp(&key, &wbt_mq_str_effect) == 0 ) {
                if( node->value.l >= 0 && node->value.l <= 2592000 ) {
                    wbt_mq_parsed_msg.effect = (unsigned int)node->value.l;
                }
            } else if ( wbt_strcmp(&key, &wbt_mq_str_expire) == 0 ) {
                if( node->value.l >= 0 && node->value.l <= 2592000 ) {
                    wbt_mq_parsed_msg.expire = (unsigned int)node->value.l;
                }
            } else if ( wbt_strcmp(&key, &wbt_mq_str_delivery_mode) == 0 ) {
                switch(node->value.l) {
                    case MSG_BROADCAST:
                        wbt_mq_parsed_msg.qos  = 0;
                        wbt_mq_parsed_msg.type = (unsigned int)node->value.l;
                        break;
                    case MSG_LOAD_BALANCE:
                        wbt_mq_parsed_msg.qos  = 1;
                        wbt_mq_parsed_msg.type = (unsigned int)node->value.l;
                        break;
                    case MSG_ACK:
                        wbt_mq_parsed_msg.qos  = 0;
                        wbt_mq_parsed_msg.type = (unsigned int)node->value.l;
                        break;
                    default:
                        break;
                }
            }
            break;
        case JSON_STRING:
        case JSON_ARRAY:
        case JSON_OBJECT:
            if ( wbt_strcmp(&key, &wbt_mq_str_data) == 0 ) {
                wbt_mq_parsed_msg.data = wbt_strdup( node->value.s, node->value_len );
                if( wbt_mq_parsed_msg.data == NULL ) {
                    wbt_mq_parsed_msg.data_len = 0;
                } else {
                    wbt_mq_parsed_msg.data_len = node->value_len;
                }
            }
            break;
        default:
            break;
    }
}

wbt_msg_t * wbt_mq_json_parser( char *data, int len ) {
    json_task_t t;
    t.str = data;
    t.len = len;
    t.callback = wbt_mq_json_parser_cb;
    
    wbt_memset(&wbt_mq_parsed_msg, 0, sizeof(wbt_mq_parsed_msg));

    if( json_parser(&t) != 0 ) {
        wbt_log_add("Message format error: %.*s\n", t.len<200 ? t.len : 200, t.str);

        return NULL;
    }

    if( !wbt_mq_parsed_msg.consumer_id || !wbt_mq_parsed_msg.data_len ) {
        return NULL;
    }

    return &wbt_mq_parsed_msg;
}

wbt_status wbt_mq_push(wbt_event_t *ev, wbt_msg_t *message) {
    if( message == NULL ) {
        // message format error
        return WBT_ERROR;
    }

    if( wbt_mq_auth_pub_limit(ev) != WBT_OK ) {
        // out of limit
        return WBT_ERROR;
    }

    if( wbt_mq_auth_pub_permission(ev, message) != WBT_OK  ) {
        // permission denied
        return WBT_ERROR;
    }
    
    // 创建消息并初始化
    wbt_msg_t * msg = wbt_mq_msg_create(0);
    if( msg == NULL ) {
        return WBT_ERROR;
    }
    
    msg->producer_id = message->producer_id;
    msg->consumer_id = message->consumer_id;
    msg->effect      = msg->create + message->effect * 1000;
    msg->expire      = msg->effect + message->expire * 1000;
    msg->type        = message->type;
    msg->qos         = message->qos;
    msg->data_len    = message->data_len;
    msg->data        = message->data;

    wbt_log_add("Message %lld received: %.*s\n", msg->msg_id, msg->data_len<200 ? msg->data_len : 200, msg->data);
    
    // 接收到消息之后，第一步是将消息持久化
    if( wbt_conf.aof ) {
        if( wbt_is_oom() && !wbt_mq_persist_aof_is_lock() ) {
            // 如果内存不足，则启动数据恢复
            wbt_mq_persist_aof_lock();

            wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
            timer->on_timeout = wbt_mq_persist_recovery;
            timer->timeout = wbt_cur_mtime;
            timer->heap_idx = 0;

            if( wbt_timer_add(timer) != WBT_OK ) {
                return WBT_ERROR;
            }
        }
        
        if( wbt_mq_persist_append(msg) != WBT_OK ) {
            // 如果持久化失败了，消息投递就会失败
            wbt_mq_msg_destory( msg );
            
            return WBT_ERROR;
        }
    } else {
        // 如果没有开启持久化，则在内存不足时删除一些旧的消息
        wbt_mq_msg_cleanup();
    }
    
    // TODO 消息持久化之后，第二步是主从复制
    // 主从复制算法根据是否开启持久化而有所区别
    if( 0 /* TODO 如果存在 slave */ ) {
        // TODO 主从同步的算法如下：
        // 1、建立连接时，slave 发送最后一次同步时收到的 msg_id
        // 2、master 根据收到的 msg_id 决定同步消息的起始位置
        // 3、master 从当前同步位置开始不断发送消息，直至最新一条消息
        // 4、当 master 收到新消息时，重复步骤 3
        
        // 如果开启了持久化，步骤 3 从 aof 文件中读取消息。
        // 如果没有开启持久化，步骤 3 从内存中读取消息
    }
    
    // 最后，进行消息投递
    if( msg->type == MSG_ACK ) {
        wbt_msg_t *msg_ack = wbt_mq_msg_get(msg->consumer_id);
        if( msg_ack ) {
            wbt_mq_msg_event_expire( &msg_ack->timer );
        }

        wbt_mq_msg_destory( msg );
    } else {
        if( wbt_mq_persist_aof_is_lock() ) {
            wbt_mq_msg_destory( msg );
        } else {
            wbt_mq_msg_delivery( msg );
        }
    }

    // TODO 返回 msg_id

    return WBT_OK;
}

wbt_status wbt_mq_pull(wbt_event_t *ev, wbt_msg_t **msg_ptr) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    //wbt_log_debug("subscriber %lld pull fron conn %d\n", subscriber->subscriber_id, ev->fd);

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
        *msg_ptr = msg;
        
        // 如果是负载均衡消息，将该消息移动到 delivered_list 中
        if( msg->type == MSG_LOAD_BALANCE ) {
            // 消息本身不能被释放
            node->value.str = NULL;
            wbt_rb_delete(&channel_node->channel->queue, node);

            wbt_msg_list_t *msg_node = wbt_mq_msg_create_node(msg);
            if( msg_node == NULL ) {
                return WBT_ERROR;
            }
            wbt_list_add_tail( &msg_node->head, &subscriber->delivered_list.head );
        }

        wbt_mq_msg_inc_delivery(msg);

        // 更新消息处理进度
        channel_node->seq_id = msg->seq_id;

        return WBT_OK;
    } else {
        *msg_ptr = NULL;

        return WBT_OK;
    }
}

wbt_status wbt_mq_set_notify(wbt_event_t *ev, wbt_status (*notify)(wbt_event_t *)) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    subscriber->notify = notify;
    return WBT_OK;
}

wbt_status wbt_mq_set_auth(wbt_event_t *ev, wbt_auth_t *auth) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    subscriber->auth = auth;
    
    return WBT_OK;
}

wbt_auth_t * wbt_mq_get_auth(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return NULL;
    }
    
    return subscriber->auth;
}