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
#include "../json/wbt_json.h"

wbt_status wbt_mq_channel_init();
wbt_channel_t * wbt_mq_channel_create(wbt_mq_id channel_id);
wbt_channel_t * wbt_mq_channel_get(wbt_mq_id channel_id);
void wbt_mq_channel_destory(wbt_channel_t *channel);

wbt_status wbt_mq_channel_add_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber);
wbt_status wbt_mq_channel_del_subscriber(wbt_channel_t *channel, wbt_subscriber_t *subscriber);

wbt_status wbt_mq_channel_add_msg(wbt_channel_t *channel, wbt_msg_t *msg);
void wbt_mq_channel_del_msg(wbt_channel_t *channel, wbt_msg_t *msg);

json_object_t * wbt_mq_channel_print(wbt_channel_t *channel);
void wbt_mq_channel_print_all(json_object_t * obj);
void wbt_mq_channel_msg_print(wbt_channel_t *channel, json_object_t * obj);
void wbt_mq_channel_subscriber_print(wbt_channel_t *channel, json_object_t * obj);

long long int wbt_mq_channel_status_active();

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_CHANNEL_H */

