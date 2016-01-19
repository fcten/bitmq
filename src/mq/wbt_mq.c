/* 
 * File:   wbt_mq.c
 * Author: fcten
 *
 * Created on 2016年1月14日, 上午10:59
 */

#include "wbt_mq.h"
#include "../common/wbt_module.h"
#include "wbt_mq_channel.h"
#include "wbt_mq_msg.h"
#include "wbt_mq_subscriber.h"

wbt_module_t wbt_module_mq = {
    wbt_string("mq"),
    wbt_mq_init, // init
    NULL, // exit
    NULL, // on_conn
    wbt_mq_on_recv, // on_recv
    NULL, // on_send
    wbt_mq_on_close  // on_close
};

// 存储所有可用频道
// 一个频道下有一个或多个消费者，如果所有消费者离开频道，该频道将会撤销
extern wbt_rbtree_t wbt_mq_channels;

// 存储所有的订阅者
extern wbt_rbtree_t wbt_mq_subscribers;

// 存储所有已接收到的消息
extern wbt_rbtree_t wbt_mq_messages;

// 存储所有未生效的消息
extern wbt_heap_t wbt_mq_message_delayed;

wbt_status wbt_mq_init() {
    wbt_rbtree_init(&wbt_mq_channels);
    wbt_rbtree_init(&wbt_mq_subscribers);
    wbt_rbtree_init(&wbt_mq_messages);

    if(wbt_heap_init(&wbt_mq_message_delayed, 1024) != WBT_OK) {
        wbt_log_add("create heap failed\n");
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_mq_on_recv(wbt_event_t *ev) {
    // 分发请求

    return WBT_OK;
}

wbt_status wbt_mq_on_close(wbt_event_t *ev) {
    // 找到该连接对应的订阅者
    wbt_subscriber_t *subscriber;

    // 遍历所有订阅的频道
        // 从该频道的 subscriber_list 中移除该订阅者
        // TODO 这需要遍历链表，对于大量订阅者的频道来说不可接受。
        // 但是负载均衡模式又需要用到链表，这个地方还需要斟酌

    // 遍历所有尚未处理完毕消息
        // 如果是负载均衡消息，将该消息重新进行投递
        // 如果是广播消息，则忽略该消息

    // 删除该订阅者
    wbt_mq_subscriber_destory(subscriber);
    
    return WBT_OK;
}

wbt_status wbt_mq_login(wbt_event_t *ev) {
    // 解析请求
    wbt_mq_id subscriber_id;

    // 创建一个新的订阅者并初始化
    wbt_subscriber_t * subscriber = wbt_mq_subscriber_create(subscriber_id);
    if( subscriber == NULL ) {
        // 返回登录失败
        return WBT_OK;
    }
    
    // 在所有想要订阅的频道的 subscriber_list 中添加该订阅者

    // 遍历想要订阅的所有频道
        // 在该频道的 subscriber_list 中添加该订阅者
        // 遍历该频道的 effective_heap
            // 复制该消息到订阅者的 msg_list 中
            // 如果是负载均衡消息，则从 effective_heap 中移除该消息

    return WBT_OK;
}

wbt_status wbt_mq_push(wbt_event_t *ev) {
    // 解析请求
    
    // 创建消息并初始化
    wbt_msg_t * msg = wbt_mq_msg_create();
    if( msg == NULL ) {
        // 返回投递失败
        return WBT_OK;
    }
    
    // 投递消息
    if( wbt_mq_msg_delivery( msg ) != WBT_OK ) {
        // 返回投递失败
        return WBT_OK;
    }
    
    // 返回投递成功 & 消息编号

    return WBT_OK;
}

wbt_status wbt_mq_pull(wbt_event_t *ev) {
    // 解析请求
    
    // 从订阅者的 msg_list 中取出第一条消息
    // 将该消息移动到 delivered_heap 中，并返回该消息
    
    // 如果没有可发送的消息，则挂起请求，设定新的超时时间和超时处理函数
    
    return WBT_OK;
}

wbt_status wbt_mq_pull_timeout(wbt_event_t *ev) {
    // 固定返回一个空的响应，通知客户端重新发起 pull 请求
    
    return WBT_OK;
}

wbt_status wbt_mq_ack(wbt_event_t *ev) {
    // 解析请求
    wbt_mq_id msg_id;
    wbt_mq_id subscriber_id;
    
    // 该条消息消费成功
    
    // 从该订阅者的 delivered_heap 中移除消息
    
    return WBT_OK;
}