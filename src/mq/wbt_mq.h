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
#include "../common/wbt_module.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_config.h"
#include "../event/wbt_event.h"

enum {
    MSG_CREATED,   // 已创建
    MSG_EFFECTIVE, // 已生效
    MSG_DELIVERED, // 已投递
    MSG_CONSUMED,  // 已消费
    MSG_EXPIRED    // 已过期
};

typedef enum wbt_msg_type_e {
    MSG_BROADCAST = 1, // 广播消息
    MSG_LOAD_BALANCE,  // 负载均衡消息
    MSG_ACK            // 确认消息
} wbt_msg_type_t;

typedef enum wbt_msg_qos_e {
    MSG_AT_LEAST_ONCE = 1,
    MSG_AT_MOST_ONCE,
    MSG_EXACTLY_ONCE
} wbt_msg_qos_t;

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
    unsigned int type:2;
    // 投递质量保证
    unsigned int qos:2;
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
    // 如果该消息正在被发送，则不能被释放
    unsigned int reference_count;
    // 消息状态
    unsigned int state:3;
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
    // 为了尽量避免妨碍消息过期，这里使用 msg_id 而不是指针
    wbt_mq_id msg_id;
} wbt_msg_list_t;

typedef struct wbt_subscriber_list_s {
    // 链表结构体
    wbt_list_t head;
    // 消息指针
    struct wbt_subscriber_s * subscriber;
} wbt_subscriber_list_t;

typedef struct wbt_channel_list_s {
    // 链表结构体
    wbt_list_t head;
    // 消息指针
    struct wbt_channel_s * channel;
    // 已处理消息序列号（小于等于该序号的消息已经被处理过）
    wbt_mq_id seq_id;
} wbt_channel_list_t;

typedef struct wbt_auth_list_s {
    // 链表结构体
    wbt_list_t head;
    // 授权 channel 区间
    wbt_mq_id min_channel_id;
    wbt_mq_id max_channel_id;
} wbt_auth_list_t;

/* 同一份授权可以由多个连接的多个订阅者共同使用。
 * 所有使用同一份授权的订阅者将共享限额。
 * 
 * 授权一旦生成，将会始终保存在内存中直至授权过期。
 * 这样可以确保即使订阅者发生断线重连也不会重置限额。
 * 
 * 授权无法被直接撤销，但可以被更新。较晚颁发的授权
 * 可以覆盖较晚颁发的授权。通过更新授权，可以实现
 * 动态调整限额，以及延长授权的有效期限。
 */
typedef struct wbt_auth_s {
    // 授权 ID
    wbt_mq_id auth_id;
    // 授权颁发时间
    time_t create;
    // 授权过期时间
    time_t expire;
    // 允许 pub 的 channel 列表
    wbt_auth_list_t pub_channels;
    // 允许 sub 的 channel 列表
    wbt_auth_list_t sub_channels;
    // 订阅者限额
    unsigned int max_subscriber;
    // 已连接的订阅者数量
    unsigned int subscriber_count;
    // 订阅者踢除策略
    // 0 - 默认：拒绝新的订阅者
    // 1 - 断开最早连接的订阅者
    //unsigned int kick_subscriber;
    // 最大消息长度（上限 64K）
    unsigned int max_msg_len;
    // 最长消息延迟时间（上限 30天）
    // 如果 BitMQ 被当作消息代理使用，建议将该值设为 0。
    unsigned int max_effect;
    // 最长消息有效时间（上限 30天）
    unsigned int max_expire;
    // 每秒发布消息限额
    unsigned int max_pub_per_second;
    // 已使用的每秒限额
    unsigned int pub_per_second;
    // 上次用完每秒限额的时间
    time_t last_pps_use_up;
    // 每分钟发布消息限额
    unsigned int max_pub_per_minute;
    // 已使用的每分钟限额
    unsigned int pub_per_minute;
    // 上次用完每分钟限额的时间
    time_t last_ppm_use_up;
    // 每小时发布消息限额
    unsigned int max_pub_per_hour;
    // 已使用的每小时限额
    unsigned int pub_per_hour;
    // 上次用完每小时限额的时间
    time_t last_pph_use_up;
    // 每天发布消息限额
    unsigned int max_pub_per_day;
    // 已使用的每天限额
    unsigned int pub_per_day;
    // 上次用完每天限额的时间
    time_t last_ppd_use_up;
    // 授权过期事件
    wbt_timer_t timer;
} wbt_auth_t;

typedef struct wbt_subscriber_s {
    // 订阅者 ID
    wbt_mq_id subscriber_id;
    // 创建时间
    time_t create;
    // TCP 连接上下文
    wbt_event_t * ev;
    // 所订阅频道 ID
    struct wbt_channel_list_s channel_list;
    // 已投递队列
    // 保存已投递但尚未返回 ACK 响应的负载均衡消息
    struct wbt_msg_list_s delivered_list;
    // 权限信息
    struct wbt_auth_s * auth;
    // 消息投递回调函数，用于通知该订阅者有消息可投递
    // 设置该方法是为了同时支持多种协议
    wbt_status (*notify)(wbt_event_t *);
} wbt_subscriber_t;

typedef struct wbt_channel_s {
    // 频道 ID
    wbt_mq_id channel_id;
    // 创建时间
    time_t create;
    // 订阅者
    struct wbt_subscriber_list_s subscriber_list;
    // 订阅者数量
    unsigned int subscriber_count;
    // 消息队列
    wbt_rb_t queue;
} wbt_channel_t;

wbt_status wbt_mq_init();
wbt_status wbt_mq_on_recv(wbt_event_t *ev);
wbt_status wbt_mq_on_close(wbt_event_t *ev);
wbt_status wbt_mq_on_success(wbt_event_t *ev);

wbt_status wbt_mq_login(wbt_event_t *ev);
wbt_status wbt_mq_subscribe(wbt_event_t *ev, wbt_mq_id channel_id);
wbt_status wbt_mq_push(wbt_event_t *ev, wbt_msg_t *msg);
wbt_status wbt_mq_pull(wbt_event_t *ev, wbt_msg_t **msg_ptr);
wbt_status wbt_mq_ack(wbt_event_t *ev);

wbt_status wbt_mq_set_notify(wbt_event_t *ev, wbt_status (*notify)(wbt_event_t *));
wbt_status wbt_mq_set_auth(wbt_event_t *ev, wbt_auth_t *auth);

wbt_auth_t * wbt_mq_get_auth(wbt_event_t *ev);

time_t wbt_mq_uptime();

wbt_msg_t * wbt_mq_json_parser( char *data, int len );

extern wbt_str_t wbt_mq_str_message;
extern wbt_str_t wbt_mq_str_channel;
extern wbt_str_t wbt_mq_str_subscriber;
extern wbt_str_t wbt_mq_str_total;
extern wbt_str_t wbt_mq_str_active;
extern wbt_str_t wbt_mq_str_delayed;
extern wbt_str_t wbt_mq_str_waiting_ack;
extern wbt_str_t wbt_mq_str_system;
extern wbt_str_t wbt_mq_str_uptime;
extern wbt_str_t wbt_mq_str_memory;
extern wbt_str_t wbt_mq_str_list;
extern wbt_str_t wbt_mq_str_msg_id;
extern wbt_str_t wbt_mq_str_channel_id;
extern wbt_str_t wbt_mq_str_subscriber_id;
extern wbt_str_t wbt_mq_str_producer_id;
extern wbt_str_t wbt_mq_str_consumer_id;
extern wbt_str_t wbt_mq_str_create;
extern wbt_str_t wbt_mq_str_effect;
extern wbt_str_t wbt_mq_str_expire;
extern wbt_str_t wbt_mq_str_delivery_mode;
extern wbt_str_t wbt_mq_str_data;
extern wbt_str_t wbt_mq_str_ip;

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_H */

