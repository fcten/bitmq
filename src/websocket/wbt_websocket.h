/* 
 * File:   wbt_websocket.h
 * Author: Fcten
 *
 * Created on 2017年01月17日, 下午4:07
 */

#ifndef __WBT_WEBSOCKET_H__
#define	__WBT_WEBSOCKET_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "../common/wbt_memory.h"
#include "../common/wbt_string.h"
#include "../common/wbt_file.h"
#include "../event/wbt_event.h"
#include "../bmtp/wbt_bmtp.h"

/*
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+
 */

typedef struct wbt_websocket_s {
    // 会话状态
    unsigned int state;
    
    unsigned int fin:1;
    unsigned int mask:1;
    unsigned int opcode:4;
    
    unsigned char mask_key[4];
    
    unsigned long long int payload_length;
    unsigned char *payload;
    
    unsigned int recv_offset;
    unsigned int msg_offset;

    wbt_bmtp_t bmtp;
} wbt_websocket_t;
    
wbt_status wbt_websocket_init();
wbt_status wbt_websocket_exit();
wbt_status wbt_websocket_on_conn( wbt_event_t *ev );
wbt_status wbt_websocket_on_close( wbt_event_t *ev );
wbt_status wbt_websocket_on_recv( wbt_event_t *ev );
wbt_status wbt_websocket_on_send( wbt_event_t *ev );

wbt_status wbt_websocket_on_handler( wbt_event_t *ev );

wbt_status wbt_websocket_send_pub(wbt_event_t *ev, char *data, unsigned int len, int qos, int dup);
wbt_status wbt_websocket_is_ready(wbt_event_t *ev);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_WEBSOCKET_H__ */

