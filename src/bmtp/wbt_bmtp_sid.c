/* 
 * File:   wbt_bmtp_sid.c
 * Author: fcten
 *
 * Created on 2016年3月25日, 下午4:20
 */

#include "../common/wbt_log.h"
#include "wbt_bmtp_sid.h"

int wbt_bmtp_sid_alloc(wbt_bmtp_t *bmtp) {
    if (!bmtp->usable_sids) {

        wbt_log_debug( "no sid left\n" );

        return 0;
    }

    unsigned char offset = bmtp->last_sid;

    unsigned int *addr = bmtp->page;
    unsigned int *p;
    unsigned int mask;

    while (1) {
        while( !++offset );

        p = addr + (offset >> 5);
        mask = 1U << (offset & 31);

        if (!((*p) & mask)) {
            *p |= mask;
            bmtp->usable_sids --;
            bmtp->last_sid = offset;

            wbt_log_debug( "alloc sid %u, %u left\n", offset, bmtp->usable_sids );

            return offset;
        }
    }

    wbt_log_debug( "no sid left\n" );

    return 0;
}

void wbt_bmtp_sid_free(wbt_bmtp_t *bmtp, unsigned int sid) {
    unsigned char offset = sid;

    unsigned int *addr = bmtp->page;
    unsigned int *p = addr + (offset >> 5);
    unsigned int mask = 1U << (offset & 31);

    *p &= ~mask;
    
    bmtp->usable_sids ++;
    
    wbt_log_debug("free sid %d\n", sid);
}
