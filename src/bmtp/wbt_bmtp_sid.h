/* 
 * File:   wbt_bmtp_sid.h
 * Author: fcten
 *
 * Created on 2016年3月25日, 下午4:20
 */

#ifndef WBT_BMTP_SID_H
#define	WBT_BMTP_SID_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "wbt_bmtp.h"

int wbt_bmtp_sid_alloc(wbt_bmtp_t *bmtp);
void wbt_bmtp_sid_free(wbt_bmtp_t *bmtp, unsigned int sid);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_BMTP_SID_H */

