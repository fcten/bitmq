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
#include "../common/wbt_rbtree.h"
#include "../common/wbt_heap.h"
#include "../common/wbt_list.h"
#include "../common/wbt_event.h"

enum {
    MSG_UNKNOWN,
    MSG_CREATED,   // 已创建
    MSG_EFFECTIVE, // 已生效
    MSG_DELIVERED, // 已投递
    MSG_CONSUMED,  // 已消费
    MSG_EXPIRED    // 已过期
};

enum {
    MSG_BROADCAST,   // 广播模式
    MSG_LOAD_BALANCE // 负载均衡模式
};

enum {
    MSG_DELIVER_ONCE,  // 至少投递一次
    MSG_CONSUME_ONCE,  // 至少消费一次
    MSG_DELIVER_TWICE, // 至少投递两次
    MSG_CONSUME_TWICE, // 至少消费两次
    MSG_DELIVER_ALL,   // 全部投递完成
    MSG_CONSUME_ALL,   // 全部消费完成
};

enum {
    MSG_
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
    // 第一次投递时间
    time_t deliver;
    // 第一次消费时间
    time_t consume;
    // 优先级
    // 数值较小的消息会被优先处理
    //unsigned int priority:4;
    // 可靠性保证
    // 1、消息投递给至少一个消费者后返回成功
    // 2、消息被至少一个消费者处理后返回成功（默认）
    // 3、消息投递给至少两个消费者后返回成功
    // 4、消息被至少两个消费者处理后返回成功
    // 5、消息投递给所有消费者后返回成功
    // 6、消息被所有消费者处理后返回成功
    unsigned int reliable:4;
    // 投递模式
    // 1、负载均衡模式：有多个消费者。消息以负载均衡的方式投递给某一个消费者。
    // 2、广播模式：有多个消费者。消息以广播的方式投递给所有消费者。
    unsigned int delivery_mode:4;
    // 投递次数
    unsigned int delivery_count;
    // 消费次数
    unsigned int consumption_count;
    // 引用次数
    unsigned int reference_count;
} wbt_msg_t;

typedef struct wbt_msg_list_s {
    // 链表结构体
    wbt_list_t list;
    // 消息指针
    wbt_msg_t * msg;
    // 消息状态
    unsigned int state:4;
} wbt_msg_list_t;

typedef struct wbt_subscriber_s {
    // 订阅者 ID
    wbt_mq_id subscriber_id;
    // 消息队列缓存
    struct wbt_msg_list_s * msg_list;
    // TCP 连接上下文
    wbt_event_t * ev;
    // 订阅频道列表
    struct wbt_channel_list_s * channel_list;
    
} wbt_subscriber_t;

typedef struct wbt_subscriber_list_s {
    // 链表结构体
    wbt_list_t list;
    // 消息指针
    wbt_subscriber_t * subscriber;
} wbt_subscriber_list_t;

typedef struct wbt_channel_s {
    // 频道 ID
    wbt_mq_id channel_id;
    // 频道类型
    // 1、临时频道
    // 如果没有订阅者，将无法向该频道投递消息
    // 如果没有订阅者并且所有消息过期，频道将会撤销。
    // 2、固定频道
    // 可以在任何时候投递消息
    // 频道永远不会撤销
    unsigned int type;
    // 订阅者
    struct wbt_subscriber_list_s * subscriber_list;
    // 已生效消息队列
    // 广播消息和未投递成功的负载均衡消息会保存在该队列中
    // 广播消息会一直保存到消息过期，未获取过该消息的订阅者将在上线后获得该消息
    // 负载均衡消息将保存到第一次投递成功，或者消息过期
    
    // 已投递消息队列
    // 保存已投递但订阅者尚未处理完毕的消息
    
} wbt_channel_t;

typedef struct wbt_channel_list_s {
    // 链表结构体
    wbt_list_t list;
    // 消息指针
    wbt_channel_t * channel;
} wbt_channel_list_t;

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_H */

