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

#include "../common/wbt_list.h"
#include "../event/wbt_event.h"

#define BMTP_VERSION 0x1

/*
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |   Protocol Version    |
 +  Byte1  +-----------------------------------------------+
 |         |  0  |  0  |  0  |  1  |  0  |  0  |  0  |  1  |
 +---------+-----------------------------------------------+
 |  Byte2  |                      'B'                      |
 +---------+-----------------------------------------------+
 |  Byte3  |                      'M'                      |
 +---------+-----------------------------------------------+
 |  Byte4  |                      'T'                      |
 +---------+-----------------------------------------------+
 |  Byte5  |                      'P'                      |
 +---------+-----------------------------------------------+
 |  Byte6  |                Payload Length                 |
 |  Byte7  |                                               |
 +---------+-----------------------------------------------+
 |  Byte8+ |                    Payload                    |
 * 
 协议版本（Protocol Version）的值必须为 0x1，如果不是，服务端应当在返回 CONNACK 后关闭连接

 BMTP_CONN 必须是连接建立后客户端发送的第一个请求，如果不是，服务端必须关闭连接
 如果一个连接重复收到 BMTP_CONN 请求，服务端必须关闭连接
*/

#define BMTP_CONN 0x10

#define wbt_bmtp_cmd(b)     (b&240)
#define wbt_bmtp_version(b) (b&15)

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |      Status Code      |
 +  Byte1  +-----------------------------------------------+
 |         |  0  |  0  |  1  |  0  |  X  |  X  |  X  |  X  |
 +---------+-----------------------------------------------+

 Status Code:
      0: 接受
      1: 拒绝，不支持的协议版本
      2: 拒绝，服务暂时不可用
      3: 拒绝，授权认证失败
      4: 拒绝，连接数过多
   5-15: 保留

 如果状态码不等于 0x0，服务端必须在发送数据包后关闭连接，客户端应当在接收数据包后关闭连接
*/

#define BMTP_CONNACK 0x20

#define wbt_bmtp_status(b) (b&15)
    
/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     | DUP |  R  |    QoS    |
 |  Byte1  +-----------------------------------------------+
 |         |  0  |  0  |  1  |  1  |  X  |  0  |  X  |  X  |
 +---------+-----------------------------------------------+
 |  Byte2  |                   Stream ID                   |
 +---------+-----------------------------------------------+
 |  Byte3  |                Payload Length                 |
 |  Byte4  |                                               |
 +---------+-----------------------------------------------+
 |  Byte5+ |                    Payload                    |

 DUP: 重发标志
 QoS: 服务质量等级 0-至多一次 1-至少一次 2-恰好一次
*/

#define BMTP_PUB 0x30

#define wbt_bmtp_dup(b)     (b&8)
#define wbt_bmtp_qos(b)     (b&3)

#define BMTP_QOS_AT_MOST_ONCE  0
#define BMTP_QOS_AT_LEAST_ONCE 1
#define BMTP_QOS_EXACTLY_ONCE  2

#define BMTP_DUP  8

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |      Status Code      |
 |  Byte1  +-----------------------------------------------+
 |         |  0  |  1  |  0  |  0  |  X  |  X  |  X  |  X  |
 +---------+-----------------------------------------------+
 |  Byte2  |                   Stream ID                   |
 +---------+-----------------------------------------------+

 Status Code:
      0: 接受
      1: 拒绝，未知错误
      2: 拒绝，服务暂时不可用
      3: 拒绝，超出速率限制
   4-15: 保留 

 */

#define BMTP_PUBACK 0x40

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |       Reserved        |
 |  Byte1  +-----------------------------------------------+
 |         |  0  |  1  |  0  |  1  |  0  |  0  |  0  |  0  |
 +---------+-----------------------------------------------+
 | Byte2-5 |                  Channel ID                   |
 +---------+-----------------------------------------------+
*/

#define BMTP_SUB 0x50

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |      Status Code      |
 |  Byte1  +-----------------------------------------------+
 |         |  0  |  1  |  1  |  0  |  X  |  X  |  X  |  X  |
 +---------+-----------------------------------------------+
 | Byte2-5 |                  Channel ID                   |
 +---------+-----------------------------------------------+
*/

#define BMTP_SUBACK 0x60

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |       Reserved        |
 |  Byte1  +-----------------------------------------------+
 |         |  1  |  1  |  0  |  0  |  0  |  0  |  0  |  0  |
 +---------+-----------------------------------------------+
 */

#define BMTP_PING 0xC0

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |       Reserved        |
 |  Byte1  +-----------------------------------------------+
 |         |  1  |  1  |  0  |  1  |  0  |  0  |  0  |  0  |
 +---------+-----------------------------------------------+
 */

#define BMTP_PINGACK 0xD0

/* 
 +---------+-----------------------------------------------+
 |   Bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 +---------+-----------------------------------------------+
 |         |      Message Type     |       Reserved        |
 |  Byte1  +-----------------------------------------------+
 |         |  1  |  1  |  1  |  0  |  0  |  0  |  0  |  0  |
 +---------+-----------------------------------------------+

 客户端或服务端发送 BMTP_DISCONN 后必须关闭连接，另一方在接收到 BMTP_DISCONN 后应当关闭连接
 */

#define BMTP_DISCONN 0xE0

typedef struct wbt_bmtp_msg_s {
    wbt_list_t head;

    unsigned int  len;
    unsigned int  offset;
    unsigned char *msg;
} wbt_bmtp_msg_t;

typedef struct {
    // 会话状态
    unsigned int  state;
    unsigned char header;
    unsigned char sid;
    unsigned long long int cid;
    unsigned int  payload_length;
    unsigned char *payload;
    
    unsigned int  recv_offset;
    
    unsigned int  is_conn:1;
    
    unsigned int  last_sid:8;
    unsigned int  usable_sids:8;
    unsigned int  page[8];
    
    wbt_bmtp_msg_t wait_queue;
    wbt_bmtp_msg_t send_queue;
    wbt_bmtp_msg_t ack_queue;
} wbt_bmtp_t;

wbt_status wbt_bmtp_init();
wbt_status wbt_bmtp_exit();

wbt_status wbt_bmtp_on_conn(wbt_event_t *ev);
wbt_status wbt_bmtp_on_recv(wbt_event_t *ev);
wbt_status wbt_bmtp_on_send(wbt_event_t *ev);
wbt_status wbt_bmtp_on_close(wbt_event_t *ev);

wbt_status wbt_bmtp_on_connect(wbt_event_t *ev);
wbt_status wbt_bmtp_on_connack(wbt_event_t *ev);
wbt_status wbt_bmtp_on_pub(wbt_event_t *ev);
wbt_status wbt_bmtp_on_puback(wbt_event_t *ev);
wbt_status wbt_bmtp_on_sub(wbt_event_t *ev);
wbt_status wbt_bmtp_on_ping(wbt_event_t *ev);
wbt_status wbt_bmtp_on_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp_on_disconn(wbt_event_t *ev);

wbt_status wbt_bmtp_send_conn(wbt_event_t *ev);
wbt_status wbt_bmtp_send_connack(wbt_event_t *ev, unsigned char status);
wbt_status wbt_bmtp_send_pub(wbt_event_t *ev, char *buf, unsigned int len, int qos, int dup);
wbt_status wbt_bmtp_send_puback(wbt_event_t *ev, unsigned char status);
wbt_status wbt_bmtp_send_sub(wbt_event_t *ev, unsigned long long int channel_id);
wbt_status wbt_bmtp_send_suback(wbt_event_t *ev, unsigned char status);
wbt_status wbt_bmtp_send_ping(wbt_event_t *ev);
wbt_status wbt_bmtp_send_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp_send_disconn(wbt_event_t *ev);
wbt_status wbt_bmtp_send(wbt_event_t *ev, char *buf, int len);

wbt_status wbt_bmtp_is_ready(wbt_event_t *ev);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_BMTP_H */

