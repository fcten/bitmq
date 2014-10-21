/* 
 * File:   wbt_connection.h
 * Author: Fcten
 *
 * Created on 2014年8月25日, 下午3:59
 */

#ifndef __WBT_CONNECTION_H__
#define	__WBT_CONNECTION_H__

#ifdef	__cplusplus
extern "C" {
#endif

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
#include "wbt_event.h"

wbt_status wbt_conn_init();
wbt_status wbt_conn_close(wbt_event_t *ev);
wbt_status wbt_conn_cleanup();

wbt_status wbt_on_connect(wbt_event_t *ev);
wbt_status wbt_on_recv(wbt_event_t *ev);
wbt_status wbt_on_send(wbt_event_t *ev);
wbt_status wbt_on_close(wbt_event_t *ev);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_CONNECTION_H__ */

