/* 
 * File:   wbt_bmtp.h
 * Author: fcten
 *
 * Created on 2016年2月26日, 下午4:42
 */

#ifndef WBT_BMTP_H
#define	WBT_BMTP_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
    CMD_UNKNOWN,
    CMD_CONN,
    CMD_PUB,
    CMD_PUBREL,
    CMD_SUB,
    CMD_UNSUB,
    CMD_PING,
    CMD_DISCONNECT,
    CMD_LENGTH
} wbt_bmtp_cmd_t;


#ifdef	__cplusplus
}
#endif

#endif	/* WBT_BMTP_H */

