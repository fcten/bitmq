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

#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "../webit.h"
#include "wbt_timer.h"
#include "wbt_event.h"

/* 视情况，可能需要管理 conn 
typedef struct wbt_conn_s {

} wbt_conn_t;
*/

wbt_status wbt_conn_init();
wbt_status wbt_conn_cleanup();

wbt_status wbt_conn_close(wbt_timer_t *timer);

wbt_status wbt_on_accept(wbt_event_t *ev);
wbt_status wbt_on_recv(wbt_event_t *ev);
wbt_status wbt_on_send(wbt_event_t *ev);
wbt_status wbt_on_close(wbt_event_t *ev);

ssize_t wbt_recv(wbt_event_t *ev, void *buf, size_t len);
ssize_t wbt_send(wbt_event_t *ev, void *buf, size_t len);

wbt_status wbt_conn_close_listen();

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_CONNECTION_H__ */

