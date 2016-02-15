/* 
 * File:   wbt_mq_status.h
 * Author: fcten
 *
 * Created on 2016年2月14日, 下午4:00
 */

#ifndef WBT_MQ_STATUS_H
#define	WBT_MQ_STATUS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

wbt_status wbt_mq_status(wbt_event_t *ev);

wbt_status wbt_mq_status_general(wbt_event_t *ev);
wbt_status wbt_mq_status_message_general(wbt_event_t *ev);
wbt_status wbt_mq_status_channel_general(wbt_event_t *ev);
wbt_status wbt_mq_status_system_general(wbt_event_t *ev);
wbt_status wbt_mq_status_message(wbt_event_t *ev);
wbt_status wbt_mq_status_channel(wbt_event_t *ev);
wbt_status wbt_mq_status_system(wbt_event_t *ev);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_MQ_STATUS_H */

