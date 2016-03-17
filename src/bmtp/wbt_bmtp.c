/* 
 * File:   wbt_bmtp.c
 * Author: fcten
 *
 * Created on 2016年2月26日, 下午4:42
 */

#include "../webit.h"
#include "../common/wbt_event.h"
#include "wbt_bmtp.h"

wbt_status wbt_bmtp_parse(wbt_bmtp_t *bmtp) {
    
}

wbt_status wbt_bmtp_on_recv(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    wbt_bmtp_parse(bmtp);
}

wbt_status wbt_bmtp_on_send(wbt_event_t *ev) {
    
}