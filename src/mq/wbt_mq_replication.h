/* 
 * File:   wbt_mq_replication.h
 * Author: fcten
 *
 * Created on 2017年6月9日, 下午7:13
 */

#ifndef WBT_MQ_REPLICATION_H
#define WBT_MQ_REPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

typedef struct wbt_repl_cli_s {
    wbt_list_t head;
    
    // 客户端对象
    wbt_subscriber_t * subscriber;
    // 消息复制进度
    wbt_mq_id msg_id;
    // 
    
} wbt_repl_cli_t;

typedef struct wbt_repl_srv_s {
    wbt_event_t * ev;
    
    struct sockaddr_in addr;
    
    wbt_timer_t reconnect_timer;
} wbt_repl_srv_t;

typedef struct wbt_repl_s {
    // 从服务端列表
    wbt_repl_cli_t client_list;
    // 从服务端数量
    unsigned int client_count;
    
} wbt_repl_t;

wbt_status wbt_mq_repl_init();

wbt_status wbt_mq_repl_on_open(wbt_event_t *ev);
wbt_status wbt_mq_repl_on_close(wbt_event_t *ev);

wbt_status wbt_mq_repl_notify(wbt_event_t *ev);

#ifdef __cplusplus
}
#endif

#endif /* WBT_MQ_REPLICATION_H */

