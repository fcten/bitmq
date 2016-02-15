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

wbt_status wbt_mq_channel_init();
wbt_channel_t * wbt_mq_channel_create(wbt_mq_id channel_id);
wbt_channel_t * wbt_mq_channel_get(wbt_mq_id channel_id);
void wbt_mq_channel_destory(wbt_channel_t *channel);

wbt_status wbt_mq_channel_add_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber);
wbt_status wbt_mq_channel_del_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber);

void wbt_mq_print_channels(wbt_str_t *resp, int maxlen);
void wbt_mq_print_channel(wbt_mq_id channel_id, wbt_str_t *resp, int maxlen);

long long int wbt_mq_channel_status_active();

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_CHANNEL_H */

