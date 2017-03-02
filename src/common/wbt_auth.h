/* 
 * File:   wbt_auth.h
 * Author: fcten
 *
 * Created on 2017年3月1日, 上午10:35
 */

#ifndef __WBT_AUTH_H__
#define __WBT_AUTH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "wbt_string.h"

//wbt_status wbt_auth_sign(wbt_str_t *token, wbt_str_t *sign);
wbt_status wbt_auth_verify(wbt_str_t *token, wbt_str_t *sign);

#ifdef __cplusplus
}
#endif

#endif /* __WBT_AUTH_H__ */

