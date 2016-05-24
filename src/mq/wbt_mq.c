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
    if( wbt_mq_persist_init() !=WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_mq_channel_init() !=WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_mq_subscriber_init() !=WBT_OK ) {
        return WBT_ERROR;
    }

    if( wbt_mq_msg_init() !=WBT_OK ) {
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

    // 重新投递尚未返回 ACK 响应的负载均衡消息
    wbt_msg_list_t *msg_node;
    wbt_msg_t *msg;
    wbt_list_for_each_entry(msg_node, wbt_msg_list_t, &subscriber->delivered_list.head, head) {
        msg = wbt_mq_msg_get(msg_node->msg_id);
        if( msg ) {
            if( wbt_mq_msg_delivery(msg) != WBT_OK ) {
                wbt_mq_msg_destory(msg);
            }
        }
    }

    // 删除该订阅者
    wbt_mq_subscriber_destory(subscriber);
    
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

wbt_status wbt_mq_parser( json_task_t * task, wbt_msg_t * msg ) {
    json_object_t * node = task->root;
    wbt_str_t key;
    
    while( node && node->object_type == JSON_OBJECT ) {
        key.str = node->key;
        key.len = node->key_len;
        switch( node->value_type ) {
            case JSON_LONGLONG:
                if ( wbt_strcmp(&key, &wbt_mq_str_consumer_id) == 0  ) {
                    msg->consumer_id = node->value.l;
                } else if ( wbt_strcmp(&key, &wbt_mq_str_producer_id) == 0 ) {
                    msg->producer_id = node->value.l;
                } else if ( wbt_strcmp(&key, &wbt_mq_str_effect) == 0 ) {
                    if( node->value.l >= 0 && node->value.l <= 2592000 ) {
                        msg->effect = (unsigned int)node->value.l;
                    }
                } else if ( wbt_strcmp(&key, &wbt_mq_str_expire) == 0 ) {
                    if( node->value.l >= 0 && node->value.l <= 2592000 ) {
                        msg->expire = (unsigned int)node->value.l;
                    }
                } else if ( wbt_strcmp(&key, &wbt_mq_str_delivery_mode) == 0 ) {
                    switch(node->value.l) {
                        case MSG_BROADCAST:
                            msg->qos  = 0;
                            msg->type = (unsigned int)node->value.l;
                            break;
                        case MSG_LOAD_BALANCE:
                            msg->qos  = 1;
                            msg->type = (unsigned int)node->value.l;
                            break;
                        case MSG_ACK:
                            msg->qos  = 0;
                            msg->type = (unsigned int)node->value.l;
                            break;
                        default:
                            return WBT_ERROR;
                    }
                }
                break;
            case JSON_STRING:
                if ( wbt_strcmp(&key, &wbt_mq_str_data) == 0 ) {
                    msg->data = wbt_strdup( node->value.s, node->value_len );
                    if( msg->data == NULL ) {
                        return WBT_ERROR;
                    }
                    msg->data_len = node->value_len;
                }
                break;
            case JSON_ARRAY:
            case JSON_OBJECT:
                if ( wbt_strcmp(&key, &wbt_mq_str_data) == 0 ) {
                    msg->data_len = 1024 * 64;
                    msg->data = wbt_malloc( msg->data_len );
                    if( msg->data == NULL ) {
                        return WBT_ERROR;
                    }
                    char *p = msg->data;
                    size_t l = msg->data_len;
                    json_print(node->value.p, &p, &l);
                    msg->data_len -= l;
                    msg->data = wbt_realloc( msg->data, msg->data_len );
                }
                break;
        }

        node = node->next;
    }

    if( !msg->consumer_id || !msg->data_len ) {
        return WBT_ERROR;
    }

    msg->effect = msg->create + msg->effect * 1000;
    msg->expire = msg->effect + msg->expire * 1000;

    return WBT_OK;
}

wbt_status wbt_mq_push(wbt_event_t *ev, char *data, int len) {
    json_task_t t;
    t.str = data;
    t.len = len;
    t.callback = NULL;

    if( json_parser(&t) != 0 ) {
        json_delete_object(t.root);
        
        return WBT_ERROR;
    }

    // 创建消息并初始化
    wbt_msg_t * msg = wbt_mq_msg_create(0);
    if( msg == NULL ) {
        json_delete_object(t.root);

        return WBT_ERROR;
    }
    
    if( wbt_mq_parser(&t, msg) != WBT_OK ) {
        json_delete_object(t.root);
        wbt_mq_msg_destory( msg );

        return WBT_ERROR;
    }
    
    json_delete_object(t.root);
    
    // 如果使用释放 TTL 最小的消息的策略
    // 如果不使用该策略，并并开启了持久化，那么该消息会暂时存储到文件中
    // 注意：消息的超时时间并不会改变，如果后续消息长时间得不到处理，可能会直接过期
    // 如果既不使用该策略，有没有开启持久化，则该条消息会被立刻删除并返回投递失败
    if( 0 ) {
        while( wbt_is_oom() ) {
            // TODO 从超时队列中找到超时时间最小的消息，并删除
        }
    }

    // 投递消息
    if( wbt_mq_persist_aof_lock() == 0 && !wbt_is_oom() ) {
        if( msg->type == MSG_ACK ) {
            // ACK 消息总是立刻被处理
            wbt_subscriber_t *subscriber = ev->ctx;
            if( subscriber == NULL || wbt_mq_subscriber_msg_ack(subscriber, msg->consumer_id) != WBT_OK ) {
                wbt_mq_msg_destory( msg );
                return WBT_ERROR;
            }

            if( wbt_conf.aof ) {
                wbt_mq_persist_append(msg, 1);
            }

            wbt_mq_msg_destory( msg );
        } else {
            if( wbt_mq_msg_delivery( msg ) != WBT_OK ) {
                if( wbt_conf.aof ) {
                    wbt_mq_persist_append(msg, 1);
                }

                wbt_mq_msg_destory( msg );
                return WBT_ERROR;
            } else {
                if( wbt_conf.aof ) {
                    wbt_mq_persist_append(msg, 1);
                }
            }
        }
    } else {
        if( msg->type == MSG_ACK ) {
            // ACK 消息总是立刻被处理
            wbt_subscriber_t *subscriber = ev->ctx;
            if( subscriber == NULL || wbt_mq_subscriber_msg_ack(subscriber, msg->consumer_id) != WBT_OK ) {
                wbt_mq_msg_destory( msg );
                return WBT_ERROR;
            }

            if( wbt_conf.aof ) {
                wbt_mq_persist_append(msg, 0);
            }

            wbt_mq_msg_destory( msg );
        } else {
            // 如果当前正在恢复数据，或者内存占用过高，则不进行投递
            // 立刻删除该消息，等待数据恢复到该消息时再进行投递
            if( !wbt_conf.aof || 0/* TODO 使用直接拒绝消息策略 */ ) {
                // 如果内存占用过高且没有开启持久化
                wbt_mq_msg_destory( msg );

                return WBT_ERROR;
            }
            wbt_mq_persist_append(msg, 0);
            wbt_mq_msg_destory( msg );

            // 如果数据恢复并没有在进行，则启动之
            if(wbt_mq_persist_aof_lock() == 0) {
                wbt_timer_t *timer = wbt_malloc(sizeof(wbt_timer_t));
                timer->on_timeout = wbt_mq_persist_recovery;
                timer->timeout = wbt_cur_mtime;
                timer->heap_idx = 0;

                if( wbt_timer_add(timer) != WBT_OK ) {
                    return WBT_ERROR;
                }
            }
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
    } else {
        *msg_ptr = NULL;

        return WBT_OK;
    }
}

wbt_status wbt_mq_set_send_cb(wbt_event_t *ev, wbt_status (*send)(wbt_event_t *, char *, unsigned int, int, int)) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    subscriber->send = send;
    return WBT_OK;
}

wbt_status wbt_mq_set_is_ready_cb(wbt_event_t *ev, wbt_status (*is_ready)(wbt_event_t *)) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    subscriber->is_ready = is_ready;
    return WBT_OK;
}
