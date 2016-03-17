/* 
 * File:   wbt_mq_dlq.h
 * Author: fcten
 *
 * Created on 2016年3月15日, 下午9:04
 */

#ifndef WBT_MQ_DLQ_H
#define	WBT_MQ_DLQ_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

// Dead Letter Queue 死信队列
//
// 成为死信的可能原因：
//     广播消息过期，且分发次数为 0
//     负载均衡消息过期，且消费次数为 0
//
// 死信处理：
//     该消息将继续保存一段时间，并投递给 producter。
//     如果没有指定 producter，则死信被立刻丢弃。
wbt_status wbt_mq_dlq_add(wbt_msg_t *msg);


#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_DLQ_H */

