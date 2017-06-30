/* 
 * File:   wbt_mq_auth.c
 * Author: fcten
 *
 * Created on 2017年2月28日, 上午11:15
 */

#include "wbt_mq_auth.h"
#include "wbt_mq_subscriber.h"
#include "json/wbt_json.h"

wbt_str_t wbt_mq_auth_str_create      = wbt_string("create");
wbt_str_t wbt_mq_auth_str_expire      = wbt_string("expire");
wbt_str_t wbt_mq_auth_pub_channels    = wbt_string("pub_channels");
wbt_str_t wbt_mq_auth_sub_channels    = wbt_string("sub_channels");
wbt_str_t wbt_mq_auth_max_subscriber  = wbt_string("max_subscriber");
wbt_str_t wbt_mq_auth_max_msg_len     = wbt_string("max_msg_len");
wbt_str_t wbt_mq_auth_max_effect      = wbt_string("max_effect");
wbt_str_t wbt_mq_auth_max_expire      = wbt_string("max_expire");
wbt_str_t wbt_mq_auth_pub_per_second  = wbt_string("pub_per_second");
wbt_str_t wbt_mq_auth_pub_per_minute  = wbt_string("pub_per_minute");
wbt_str_t wbt_mq_auth_pub_per_hour    = wbt_string("pub_per_hour");
wbt_str_t wbt_mq_auth_pub_per_day     = wbt_string("pub_per_day");

// 存储所有可用频道
static wbt_rb_t wbt_mq_auths;

wbt_status wbt_mq_auth_add_pub_channels(wbt_auth_t * auth, wbt_mq_id min, wbt_mq_id max) {
    wbt_auth_list_t *list = wbt_calloc(sizeof(wbt_auth_list_t));
    if( list == NULL ) {
        return WBT_ERROR;
    }
    
    list->min_channel_id = min;
    list->max_channel_id = max;
    
    wbt_list_add(&list->head, &auth->pub_channels.head);

    return WBT_OK;
}

wbt_status wbt_mq_auth_add_sub_channels(wbt_auth_t * auth, wbt_mq_id min, wbt_mq_id max) {
    wbt_auth_list_t *list = wbt_calloc(sizeof(wbt_auth_list_t));
    if( list == NULL ) {
        return WBT_ERROR;
    }
    
    list->min_channel_id = min;
    list->max_channel_id = max;
    
    wbt_list_add(&list->head, &auth->sub_channels.head);

    return WBT_OK;
}

wbt_status wbt_mq_auth_parser( json_task_t * task, wbt_auth_t * auth ) {
    json_object_t * node = task->root;
    wbt_str_t key;
    
    while( node && node->object_type == JSON_OBJECT ) {
        key.str = node->key;
        key.len = node->key_len;
        switch( node->value_type ) {
            case JSON_LONGLONG:
                if( wbt_strcmp(&key, &wbt_mq_auth_max_subscriber) == 0 ) {
                    auth->max_subscriber = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_max_msg_len) == 0 ) {
                    auth->max_msg_len = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_max_effect) == 0 ) {
                    auth->max_effect = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_max_expire) == 0 ) {
                    auth->max_expire = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_pub_per_second) == 0 ) {
                    auth->max_pub_per_second = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_pub_per_minute) == 0 ) {
                    auth->max_pub_per_minute = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_pub_per_hour) == 0 ) {
                    auth->max_pub_per_hour = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_pub_per_day) == 0 ) {
                    auth->max_pub_per_day = (unsigned int)node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_str_create) == 0 ) {
                    auth->create = node->value.l;
                } else if( wbt_strcmp(&key, &wbt_mq_auth_str_expire) == 0 ) {
                    auth->expire = node->value.l;
                }
                break;
            case JSON_ARRAY:
                if( wbt_strcmp(&key, &wbt_mq_auth_pub_channels) == 0 ) {
                    json_object_t * children = node->value.p;
                    while( children && children->object_type == JSON_ARRAY ) {
                        if( children->value_type == JSON_ARRAY &&
                            children->value.p->value_type == JSON_LONGLONG &&
                            children->value.p->next != NULL &&
                            children->value.p->next->value_type == JSON_LONGLONG ) {
                            if( wbt_mq_auth_add_pub_channels(
                                    auth,
                                    children->value.p->value.l,
                                    children->value.p->next->value.l
                                ) != WBT_OK ) {
                                return WBT_ERROR;
                            }
                        }

                        children = children->next;
                    }
                } else if( wbt_strcmp(&key, &wbt_mq_auth_sub_channels) == 0 ) {
                    json_object_t * children = node->value.p;
                    while( children && children->object_type == JSON_ARRAY ) {
                        if( children->value_type == JSON_ARRAY &&
                            children->value.p->value_type == JSON_LONGLONG &&
                            children->value.p->next != NULL &&
                            children->value.p->next->value_type == JSON_LONGLONG ) {
                            if( wbt_mq_auth_add_sub_channels(
                                    auth,
                                    children->value.p->value.l,
                                    children->value.p->next->value.l
                                ) != WBT_OK ) {
                                return WBT_ERROR;
                            }
                        }

                        children = children->next;
                    }
                }
                break;
        }
        
        node = node->next;
    }
    
    // 参数校验
    if( auth->create >= auth->expire ) {
        return WBT_ERROR;
    }

    if( wbt_cur_mtime >= auth->expire ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

static wbt_auth_t *anonymous;

wbt_status wbt_mq_auth_init() {
    wbt_rb_init(&wbt_mq_auths, WBT_RB_KEY_LONGLONG);

    // 加载匿名授权
    if( wbt_conf.auth_anonymous.len > 0 ) {
        anonymous = wbt_mq_auth_create(&wbt_conf.auth_anonymous);
        if( anonymous == NULL ) {
            wbt_log_add("invalid auth_anonymous token\n");
            return WBT_ERROR;
        }
    } else {
        anonymous = NULL;
    }

    return WBT_OK;
}

/* 获取匿名授权
 * 如果没有启用匿名授权 则返回 NULL
 */
wbt_auth_t * wbt_mq_auth_anonymous() {
    return anonymous;
}

wbt_status wbt_mq_auth_expired(wbt_timer_t *timer) {
    wbt_auth_t *auth = wbt_timer_entry(timer, wbt_auth_t, timer);

    if( auth->subscriber_count > 0 ) {
        timer->timeout = wbt_cur_mtime + 60 * 1000;
        if( wbt_timer_mod(timer) != WBT_OK ) {
            return WBT_ERROR;
        }
    } else {
        wbt_mq_auth_destory(auth);
    }
    
    return WBT_OK;
}

/* 通过授权 token 创建或更新一份授权
 */
wbt_auth_t * wbt_mq_auth_create(wbt_str_t *token) {
    json_task_t t;
    t.str = token->str;
    t.len = token->len;
    t.callback = NULL;
    
    if( json_parser(&t) != 0 ) {
        json_delete_object(t.root);
        wbt_log_add("Token format error: %.*s\n", token->len<200 ? token->len : 200, token->str);
        return NULL;
    }

    wbt_auth_t * auth = wbt_calloc(sizeof(wbt_auth_t));
    if( auth == NULL ) {
        json_delete_object(t.root);
        return NULL;
    }
    
    wbt_list_init(&auth->pub_channels.head);
    wbt_list_init(&auth->sub_channels.head);
    
    if( wbt_mq_auth_parser(&t, auth) != WBT_OK ) {
        json_delete_object(t.root);
        wbt_log_add("Token attribute error: %.*s\n", token->len<200 ? token->len : 200, token->str);
        wbt_free( auth );
        return NULL;
    }

    wbt_log_add("Token %lld created: %.*s\n", auth->auth_id, token->len<200 ? token->len : 200, token->str);
    
    json_delete_object(t.root);
    
    wbt_auth_t *old_auth = wbt_mq_auth_get(auth->auth_id);
    if( old_auth == NULL ) {
        // 如果该授权尚未记录，创建一份新的授权
        wbt_str_t auth_key;
        wbt_variable_to_str(auth->auth_id, auth_key);
        wbt_rb_node_t * auth_node = wbt_rb_insert(&wbt_mq_auths, &auth_key);
        if( auth_node == NULL ) {
            wbt_free(auth);
            return NULL;
        }
        auth_node->value.str = (char *)auth;
        
        auth->timer.on_timeout = wbt_mq_auth_expired;
        auth->timer.timeout    = auth->expire;
        auth->timer.heap_idx   = 0;
        if( wbt_timer_add(&auth->timer) != WBT_OK ) {
            return NULL;
        }
        
        old_auth = auth;
    } else {
        // 如果该授权已经存在
        if( old_auth->create > auth->create ||
            old_auth->expire > auth->expire ) {
            // 不允许使用较早颁发的授权，也不允许提早授权过期时间
            return NULL;
        }
        
        if( old_auth->create == auth->create &&
            old_auth->expire == auth->expire ) {
            // 颁发时间和过期时间完全相同的授权被视为统一份授权
        } else {
            // 更新授权
            old_auth->timer.timeout = auth->expire;
            if( wbt_timer_mod(&old_auth->timer) != WBT_OK ) {
                return NULL;
            }

            old_auth->create = auth->create;
            old_auth->expire = auth->expire;
            //old_auth->kick_subscriber = auth->kick_subscriber;
            old_auth->max_effect = auth->max_effect;
            old_auth->max_expire = auth->max_expire;
            old_auth->max_msg_len = auth->max_msg_len;
            old_auth->max_pub_per_day = auth->max_pub_per_day;
            old_auth->max_pub_per_hour = auth->max_pub_per_hour;
            old_auth->max_pub_per_minute = auth->max_pub_per_minute;
            old_auth->max_pub_per_second = auth->max_pub_per_second;
            old_auth->max_subscriber = auth->max_subscriber;
            
            wbt_auth_list_t * pos;
            while(!wbt_list_empty(&old_auth->pub_channels.head)) {
                pos = wbt_list_first_entry(&old_auth->pub_channels.head, wbt_auth_list_t, head);
                wbt_list_del(&pos->head);
                wbt_free(pos);
            }
            while(!wbt_list_empty(&old_auth->sub_channels.head)) {
                pos = wbt_list_first_entry(&old_auth->sub_channels.head, wbt_auth_list_t, head);
                wbt_list_del(&pos->head);
                wbt_free(pos);
            }
            while(!wbt_list_empty(&auth->pub_channels.head)) {
                pos = wbt_list_first_entry(&old_auth->pub_channels.head, wbt_auth_list_t, head);
                wbt_list_del(&pos->head);
                wbt_list_add(&pos->head, &old_auth->pub_channels.head);
            }
            while(!wbt_list_empty(&auth->sub_channels.head)) {
                pos = wbt_list_first_entry(&old_auth->sub_channels.head, wbt_auth_list_t, head);
                wbt_list_del(&pos->head);
                wbt_list_add(&pos->head, &old_auth->sub_channels.head);
            }
        }
        
        wbt_free(auth);
    }
    
    return old_auth;
}

wbt_auth_t * wbt_mq_auth_get(wbt_mq_id auth_id) {
    wbt_str_t auth_key;
    wbt_variable_to_str(auth_id, auth_key);
    wbt_rb_node_t *auth_node = wbt_rb_get(&wbt_mq_auths, &auth_key);

    if( auth_node == NULL ) {
        return NULL;
    } else {
        return (wbt_auth_t *)auth_node->value.str;
    }
}

void wbt_mq_auth_destory(wbt_auth_t *auth) {
    if( auth == NULL ) {
        return;
    }

    wbt_auth_list_t *pos;
    while(!wbt_list_empty(&auth->pub_channels.head)) {
        pos = wbt_list_first_entry(&auth->pub_channels.head, wbt_auth_list_t, head);
        wbt_list_del(&pos->head);
        wbt_free(pos);
    }

    while(!wbt_list_empty(&auth->sub_channels.head)) {
        pos = wbt_list_first_entry(&auth->sub_channels.head, wbt_auth_list_t, head);
        wbt_list_del(&pos->head);
        wbt_free(pos);
    }
    
    wbt_timer_del(&auth->timer);
    
    wbt_str_t auth_key;
    wbt_variable_to_str(auth->auth_id, auth_key);
    wbt_rb_node_t *auth_node = wbt_rb_get( &wbt_mq_auths, &auth_key );
    if( auth_node ) {
        wbt_rb_delete( &wbt_mq_auths, auth_node );
    }
}

wbt_status wbt_mq_auth_disconn(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_auth_t *auth = subscriber->auth;
    if( auth == NULL ) {
        return WBT_OK;
    }
    
    auth->subscriber_count --;
    
    return WBT_OK;
}

wbt_status wbt_mq_auth_conn_limit(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_auth_t *auth = subscriber->auth;
    if( auth == NULL ) {
        return WBT_OK;
    }

    auth->subscriber_count ++;

    if( auth->subscriber_count > auth->max_subscriber ) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_mq_auth_pub_limit(wbt_event_t *ev) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_auth_t *auth = subscriber->auth;
    if( auth == NULL ) {
        return WBT_OK;
    }

    if( auth->pub_per_second >= auth->max_pub_per_second ) {
        if( wbt_cur_mtime - auth->last_pps_use_up < 1000 ) {
            return WBT_ERROR;
        } else {
            auth->last_pps_use_up = wbt_cur_mtime;
            auth->pub_per_second = 0;
        }
    }

    if( auth->pub_per_minute >= auth->max_pub_per_minute ) {
        if( wbt_cur_mtime - auth->last_ppm_use_up < 60 * 1000 ) {
            return WBT_ERROR;
        } else {
            auth->last_ppm_use_up = wbt_cur_mtime;
            auth->pub_per_minute = 0;
        }
    }

    if( auth->pub_per_hour >= auth->max_pub_per_hour ) {
        if( wbt_cur_mtime - auth->last_pph_use_up < 60 * 60 * 1000 ) {
            return WBT_ERROR;
        } else {
            auth->last_pph_use_up = wbt_cur_mtime;
            auth->pub_per_hour = 0;
        }
    }

    if( auth->pub_per_day >= auth->max_pub_per_day ) {
        if( wbt_cur_mtime - auth->last_ppd_use_up < 24 * 60 * 60 * 1000 ) {
            return WBT_ERROR;
        } else {
            auth->last_ppd_use_up = wbt_cur_mtime;
            auth->pub_per_day = 0;
        }
    }

    auth->pub_per_day ++;
    auth->pub_per_hour ++;
    auth->pub_per_minute ++;
    auth->pub_per_second ++;
    
    return WBT_OK;
}

wbt_status wbt_mq_auth_sub_permission(wbt_event_t *ev, wbt_mq_id channel_id) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_auth_t *auth = subscriber->auth;
    if( auth == NULL ) {
        return WBT_OK;
    }
    
    wbt_auth_list_t *node;
    wbt_list_for_each_entry(node, wbt_auth_list_t, &auth->sub_channels.head, head) {
        if( channel_id >= node->min_channel_id &&
            channel_id <= node->max_channel_id ) {
            return WBT_OK;
        }
    }
    
    return WBT_ERROR;
}

wbt_status wbt_mq_auth_pub_permission(wbt_event_t *ev, wbt_msg_t *msg) {
    wbt_subscriber_t *subscriber = ev->ctx;
    if( subscriber == NULL ) {
        return WBT_ERROR;
    }
    
    if( msg->type == MSG_ACK ) {
        // 这里不仅检查了 ACK 消息的权限，同时也对 ACK 消息进行了处理
        if( wbt_mq_subscriber_msg_ack(subscriber, msg->consumer_id ) != WBT_OK ) {
            return WBT_ERROR;
        } else {
            return WBT_OK;
        }
    }

    wbt_auth_t *auth = subscriber->auth;
    if( auth == NULL ) {
        return WBT_OK;
    }

    int is_ok = 0;
    wbt_auth_list_t *node;
    wbt_list_for_each_entry(node, wbt_auth_list_t, &auth->pub_channels.head, head) {
        if( msg->consumer_id >= node->min_channel_id &&
            msg->consumer_id <= node->max_channel_id ) {
            is_ok = 1;
            break;
        }
    }
    
    if( !is_ok ) {
        return WBT_ERROR;
    }
    
    if( auth->max_effect < msg->effect ) {
        return WBT_ERROR;
    }
    
    if( auth->max_expire < msg->expire ) {
        return WBT_ERROR;
    }
    
    if( auth->max_msg_len < msg->data_len ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}