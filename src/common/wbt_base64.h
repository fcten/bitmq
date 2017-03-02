/* 
 * File:   wbt_base64.h
 * Author: fcten
 *
 * Created on 2017年3月1日, 上午9:44
 */

#ifndef __WBT_BASE64_H__
#define __WBT_BASE64_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "wbt_string.h"

void wbt_base64_encode(wbt_str_t *dst, wbt_str_t *src);
void wbt_base64_encode_url(wbt_str_t *dst, wbt_str_t *src);

wbt_status wbt_base64_decode(wbt_str_t *dst, wbt_str_t *src);
wbt_status wbt_base64_decode_url(wbt_str_t *dst, wbt_str_t *src);

#ifdef __cplusplus
}
#endif

#endif /* __WBT_BASE64_H__ */

