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

wbt_msg_t * wbt_mq_msg_create();
void wbt_mq_msg_destory(wbt_msg_t *msg);
wbt_status wbt_mq_msg_delivery(wbt_msg_t *msg);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_MSG_H */

