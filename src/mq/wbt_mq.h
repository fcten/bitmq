/* 
 * File:   wbt_mq.h
 * Author: fcten
 *
 * Created on 2016年1月14日, 上午10:59
 */

#ifndef WBT_MQ_H
#define	WBT_MQ_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "../common/wbt_log.h"
#include "../common/wbt_time.h"
#include "../common/wbt_timer.h"
#include "../common/wbt_rbtree.h"
#include "../common/wbt_heap.h"
#include "../common/wbt_list.h"
#include "../common/wbt_event.h"
#include "../common/wbt_module.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_config.h"
    
enum {
    MSG_CREATED,   // 已创建
    MSG_EFFECTIVE, // 已生效
    MSG_DELIVERED, // 已投递
    MSG_CONSUMED,  // 已消费
    MSG_EXPIRED    // 已过期
};

enum {
    MSG_BROADCAST, // 广播模式
    MSG_LOAD_BALANCE   // 负载均衡模式
};

typedef long long unsigned int wbt_mq_id;

typedef struct wbt_msg_s {
    // 消息 ID
    wbt_mq_id msg_id;
    // 消息生产者所订阅的频道 ID
    wbt_mq_id producer_id;
    // 消息消费者所订阅的频道 ID
    wbt_mq_id consumer_id;
    // 创建时间
    time_t create;
    // 生效时间
    time_t effect;
    // 过期时间
    time_t expire;
    // 优先级
    // 数值较小的消息会被优先处理
    //unsigned int priority:4;
    // 投递模式
    // 1、负载均衡模式：有多个消费者。消息以负载均衡的方式投递给某一个消费者。
    // 2、广播模式：有多个消费者。消息以广播的方式投递给所有消费者。
    unsigned int delivery_mode:4;
    // 消息长度
    size_t data_len;
    // 消息生效顺序
    wbt_mq_id seq_id;
    // 超时事件
    wbt_timer_t timer;
    // 投递次数
    unsigned int delivery_count;
    // 消费次数
    unsigned int consumption_count;
    // 引用次数
    // 如果该消息正在被发送，或者正在等待 ACK 响应，则不能被释放
    //unsigned int reference_count;
    // 消息内容
    void * data;
} wbt_msg_t;

typedef struct wbt_msg_list_s {
    // 链表结构体
    wbt_list_t head;
    // 消息指针
    //wbt_msg_t * msg;
    // 过期时间
    //time_t expire;
    // 消息 ID
    // 使用指针效率更高，但是必须在内存中保存所有消息的索引，会对消息持久化造成障碍
    wbt_mq_id msg_id;
} wbt_msg_list_t;

typedef struct wbt_subscriber_s {
    // 订阅者 ID
    wbt_mq_id subscriber_id;
    // 创建时间
    time_t create;
    // TCP 连接上下文
    wbt_event_t * ev;
    // 所订阅频道 ID
    struct wbt_channel_list_s * channel_list;
    // 当前正在处理的消息 ID
    wbt_mq_id msg_id;
    // 已投递消息队列
    // 保存已投递但尚未返回 ACK 响应的负载均衡消息
    struct wbt_msg_list_s * delivered_list;
} wbt_subscriber_t;

typedef struct wbt_subscriber_list_s {
    // 链表结构体
    wbt_list_t head;
    // 消息指针
    wbt_subscriber_t * subscriber;
} wbt_subscriber_list_t;

typedef struct wbt_channel_s {
    // 频道 ID
    wbt_mq_id channel_id;
    // 创建时间
    time_t create;
    // 订阅者
    struct wbt_subscriber_list_s * subscriber_list;
    // 订阅者数量
    unsigned int subscriber_count;
    // 消息队列
    wbt_rbtree_t queue;
} wbt_channel_t;

typedef struct wbt_channel_list_s {
    // 链表结构体
    wbt_list_t head;
    // 消息指针
    wbt_channel_t * channel;
    // 已处理消息序列号（小于等于该序号的消息已经被处理过）
    wbt_mq_id seq_id;
} wbt_channel_list_t;

wbt_status wbt_mq_init();
wbt_status wbt_mq_on_recv(wbt_event_t *ev);
wbt_status wbt_mq_on_close(wbt_event_t *ev);
wbt_status wbt_mq_on_success(wbt_event_t *ev);

wbt_status wbt_mq_login(wbt_event_t *ev);
wbt_status wbt_mq_push(wbt_event_t *ev);
wbt_status wbt_mq_pull(wbt_event_t *ev);
wbt_status wbt_mq_pull_timeout(wbt_timer_t *timer);
wbt_status wbt_mq_ack(wbt_event_t *ev);

time_t wbt_mq_uptime();

extern wbt_str_t wbt_str_message;
extern wbt_str_t wbt_str_channel;
extern wbt_str_t wbt_str_subscriber;
extern wbt_str_t wbt_str_total;
extern wbt_str_t wbt_str_active;
extern wbt_str_t wbt_str_delayed;
extern wbt_str_t wbt_str_waiting_ack;
extern wbt_str_t wbt_str_system;
extern wbt_str_t wbt_str_uptime;
extern wbt_str_t wbt_str_channel_id;
extern wbt_str_t wbt_str_list;

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_H */

