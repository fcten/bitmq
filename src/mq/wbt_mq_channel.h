/* 
 * File:   wbt_mq_channel.h
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:32
 */

#ifndef WBT_MQ_CHANNEL_H
#define	WBT_MQ_CHANNEL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

wbt_channel_t * wbt_mq_channel_create(wbt_mq_id channel_id);
wbt_channel_t * wbt_mq_channel_get(wbt_mq_id channel_id);
void wbt_mq_channel_destory(wbt_channel_t *channel);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_CHANNEL_H */

