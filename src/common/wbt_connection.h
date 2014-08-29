/* 
 * File:   wbt_connection.h
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午3:59
 */

#ifndef __WBT_CONNECTION_H__
#define	__WBT_CONNECTION_H__


#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "../webit.h"

#define WBT_MAX_EVENTS      256
#define WBT_EVENT_LIST_SIZE 1024
#define WBT_CONN_PORT       1039
#define WBT_CONN_BACKLOG    511
#define WBT_CONN_TIMEOUT    5   /* 单位秒 */

wbt_status wbt_conn_init();


#endif	/* __WBT_CONNECTION_H__ */

