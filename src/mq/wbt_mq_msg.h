/* 
 * File:   wbt_mq_msg.h
 * Author: fcten
 *
 * Created on 2016年1月18日, 下午4:08
 */

#ifndef WBT_MQ_MSG_H
#define	WBT_MQ_MSG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

wbt_status wbt_mq_msg_init();
wbt_msg_t * wbt_mq_msg_create();
void wbt_mq_msg_destory(wbt_msg_t *msg);
wbt_status wbt_mq_msg_delivery(wbt_msg_t *msg);

wbt_msg_list_t * wbt_mq_msg_create_node(wbt_msg_t *msg);
void wbt_mq_msg_destory_node(wbt_msg_list_t *node);

static inline void wbt_mq_msg_inc_refer(wbt_msg_t *msg) {
    msg->reference_count ++;
    wbt_log_debug("msg %lld, refer inc, %d", msg->msg_id, msg->reference_count);
}

static inline void wbt_mq_msg_dec_refer(wbt_msg_t *msg) {
    msg->reference_count --;
    wbt_log_debug("msg %lld, refer dec, %d", msg->msg_id, msg->reference_count);
}

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_MSG_H */

