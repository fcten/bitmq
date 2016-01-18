/* 
 * File:   wbt_mq_subscriber.c
 * Author: fcten
 *
 * Created on 2016年1月16日, 上午12:21
 */

#include "wbt_mq_subscriber.h"

// 存储所有的订阅者
wbt_rbtree_t wbt_mq_subscribers;