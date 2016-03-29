/* 
 * File:   wbt_bmtp_sid.c
 * Author: fcten
 *
 * Created on 2016年3月25日, 下午4:20
 */

#include "wbt_bmtp_sid.h"

int wbt_bmtp_sid_alloc(wbt_bmtp_t *bmtp) {
    if (!bmtp->usable_sids) {
        return -1;
    }

    unsigned char offset = bmtp->last_sid + 1;

    unsigned int *addr = bmtp->page;
    unsigned int *p;
    unsigned int mask;

    while (1) {
        p = addr + (offset >> 5);
        mask = 1U << (offset & 31);

        if (!((*p) & mask)) {
            *p |= mask;
            bmtp->usable_sids --;
            bmtp->last_sid = offset;
            return offset;
        }

        ++ offset;
    }

    return -1;
}

void wbt_bmtp_sid_free(wbt_bmtp_t *bmtp, unsigned int sid) {
    unsigned char offset = sid;

    unsigned int *addr = bmtp->page;
    unsigned int *p = addr + (offset >> 5);
    unsigned int mask = 1U << (offset & 31);

    *p &= ~mask;
    
    bmtp->usable_sids ++;
}