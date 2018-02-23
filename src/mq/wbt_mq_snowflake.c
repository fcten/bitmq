/* 
 * File:   wbt_mq_snowflake.c
 * Author: fcten
 *
 * Created on 2018年2月23日, 上午11:51
 */

#include "wbt_mq_snowflake.h"

void wbt_mq_init_id(wbt_sf_t *sf, int worker_id) {
    sf->timestamp = wbt_cur_mtime - SF_EPOCH;
    sf->worker_id = worker_id;
    sf->seq_id    = 0;
}

void wbt_mq_set_id(wbt_sf_t *sf, wbt_mq_id msg_id) {
    sf->timestamp = msg_id >> 22;
    sf->worker_id = msg_id >> 12;
    sf->seq_id    = msg_id;
}

void wbt_mq_update_id(wbt_sf_t *sf, wbt_mq_id msg_id) {
    wbt_sf_t tmp;
    wbt_mq_set_id(&tmp, msg_id);
    
    if( tmp.worker_id == sf->worker_id ) {
        if( msg_id > wbt_mq_cur_id(sf) ) {
            wbt_mq_set_id(sf, msg_id);
        }
    }
}

wbt_mq_id wbt_mq_next_id(wbt_sf_t *sf) {
    time_t cur_time = wbt_cur_mtime - SF_EPOCH;
    
    if( cur_time > sf->timestamp ) {
        sf->timestamp = cur_time;
        sf->seq_id = 0;
    } else if( cur_time > sf->timestamp - SF_MAX_ERR_RANGE ) {
        if( sf->seq_id < SF_MAX_SEQ_ID ) {
            sf->seq_id ++;
        } else {
            // 借用下一毫秒
            sf->timestamp ++;
            sf->seq_id = 0;
        }
    } else {
        return 0;
    }
    
    return wbt_mq_cur_id(sf);
}