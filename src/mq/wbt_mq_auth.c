/* 
 * File:   wbt_mq_auth.c
 * Author: fcten
 *
 * Created on 2017年2月28日, 上午11:15
 */

#include "wbt_mq_auth.h"
#include "json/wbt_json.h"

wbt_str_t wbt_mq_auth_pub_channels    = wbt_string("pub_channels");
wbt_str_t wbt_mq_auth_sub_channels    = wbt_string("sub_channels");
wbt_str_t wbt_mq_auth_max_subscriber  = wbt_string("max_subscriber");
wbt_str_t wbt_mq_auth_kick_subscriber = wbt_string("kick_subscriber");
wbt_str_t wbt_mq_auth_max_msg_len     = wbt_string("max_msg_len");
wbt_str_t wbt_mq_auth_max_effect      = wbt_string("max_effect");
wbt_str_t wbt_mq_auth_max_expire      = wbt_string("max_expire");
wbt_str_t wbt_mq_auth_pub_per_second  = wbt_string("pub_per_second");
wbt_str_t wbt_mq_auth_pub_per_minute  = wbt_string("pub_per_minute");
wbt_str_t wbt_mq_auth_pub_per_hour    = wbt_string("pub_per_hour");
wbt_str_t wbt_mq_auth_pub_per_day     = wbt_string("pub_per_day");

// 存储所有可用频道
static wbt_rb_t wbt_mq_auths;

void wbt_mq_auth_parser( json_task_t * task, json_object_t * node ) {
    if( node->next != NULL ) return;
    
    wbt_auth_t * auth = (wbt_auth_t *)task->root;
    
    wbt_str_t key;
    key.len = node->key_len;
    key.str = node->key;
    
    switch(node->value_type) {
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
            }
            break;
        case JSON_STRING:
            break;
    }
}

wbt_status wbt_mq_auth_init() {
    wbt_rb_init(&wbt_mq_auths, WBT_RB_KEY_LONGLONG);

    // 加载匿名授权
    //wbt_mq_auth_create();

    return WBT_OK;
}

/* 通过授权 token 创建或更新一份授权
 */
wbt_auth_t * wbt_mq_auth_create(wbt_str_t *token) {
    wbt_auth_t * auth = wbt_calloc(sizeof(wbt_auth_t));
    if( auth == NULL ) {
        return NULL;
    }
    
    wbt_list_init(&auth->pub_channels.head);
    wbt_list_init(&auth->sub_channels.head);
    
    json_task_t t;
    t.str = token->str;
    t.len = token->len;
    t.callback = wbt_mq_auth_parser;
    t.root = (json_object_t *)auth;

    if( json_parser(&t) != 0 ) {
        wbt_free(auth);
        wbt_log_add("Token format error: %.*s\n", token->len<200 ? token->len : 200, token->str);
        return NULL;
    }
    
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
        old_auth = auth;
    } else {
        // 如果该授权已经存在
        if( old_auth->create > auth->create ||
            old_auth->expire > auth->expire ) {
            // 不允许使用较早颁发的授权，也不允许提早授权过期时间
            return NULL;
        }
        
        // 更新授权
        old_auth->timer.timeout = auth->expire;
        if( wbt_timer_mod(&old_auth->timer) != WBT_OK ) {
            return NULL;
        }

        old_auth->create = auth->create;
        old_auth->expire = auth->expire;
        old_auth->kick_subscriber = auth->kick_subscriber;
        old_auth->max_effect = auth->max_effect;
        old_auth->max_expire = auth->max_expire;
        old_auth->max_msg_len = auth->max_msg_len;
        old_auth->max_pub_per_day = auth->max_pub_per_day;
        old_auth->max_pub_per_hour = auth->max_pub_per_hour;
        old_auth->max_pub_per_minute = auth->max_pub_per_minute;
        old_auth->max_pub_per_second = auth->max_pub_per_second;
        old_auth->max_subscriber = auth->max_subscriber;
        //old_auth->pub_channels
        //old_auth->sub_channels
        
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