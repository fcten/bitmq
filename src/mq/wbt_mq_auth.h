/* 
 * File:   wbt_mq_auth.h
 * Author: fcten
 *
 * Created on 2017年2月28日, 上午11:15
 */

#ifndef WBT_MQ_AUTH_H
#define WBT_MQ_AUTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wbt_mq.h"

wbt_status wbt_mq_auth_init();
wbt_auth_t * wbt_mq_auth_create(wbt_str_t *token);
wbt_auth_t * wbt_mq_auth_get(wbt_mq_id auth_id);
void wbt_mq_auth_destory(wbt_auth_t *auth);

#ifdef __cplusplus
}
#endif

#endif /* WBT_MQ_AUTH_H */

