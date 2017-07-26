/* 
 * File:   wbt_bmtp2.h
 * Author: fcten
 *
 * Created on 2017年6月19日, 下午9:31
 */

#ifndef WBT_BMTP2_H
#define WBT_BMTP2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../webit.h"
#include "../event/wbt_event.h"
#include "../common/wbt_list.h"
#include "../common/wbt_rbtree.h"
#include "../common/wbt_string.h"
#include "../common/wbt_module.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_config.h"
#include "../common/wbt_log.h"
#include "../common/wbt_auth.h"
#include "../common/wbt_base64.h"
#include "../json/wbt_json.h"
#include "../mq/wbt_mq.h"
#include "../mq/wbt_mq_msg.h"
#include "../mq/wbt_mq_auth.h"

enum {
    TYPE_BOOL = 0,
    TYPE_VARINT,
    TYPE_64BIT,
    TYPE_STRING
};

enum {
    OP_RSV = 0,
    OP_CONN,
    OP_CONNACK,
    OP_PUB,
    OP_PUBACK,
    OP_SUB,
    OP_SUBACK,
    OP_PING,
    OP_PINGACK,
    OP_DISCONN,
    OP_WINDOW,
    OP_SYNC = 16,
    OP_MAX
};

enum {
    HEAD_END = 0,
    HEAD_PAYLOAD,
    HEAD_STREAM_ID,
    HEAD_STATUS_CODE,
    HEAD_EXT = 16
};

typedef struct {
    unsigned int opcode;
    wbt_str_t name;
    wbt_status (*on_proc)( wbt_event_t *ev );
} wbt_bmtp2_cmd_t;

typedef struct wbt_bmtp2_param_s {
    unsigned int key;
    unsigned int key_type:3;
    struct {
        unsigned char *s;
        unsigned long long int l;
    } value;
} wbt_bmtp2_param_t;

typedef struct wbt_bmtp2_msg_list_s {
    wbt_list_t head;

    unsigned char *hed;
    unsigned int   hed_length;
    unsigned int   hed_start;
    unsigned int   hed_offset;
    
    wbt_msg_t     *msg;
    unsigned int   msg_offset;
} wbt_bmtp2_msg_list_t;

enum {
    BMTP_SERVER = 1,
    BMTP_CLIENT
};

typedef struct {
    // 客户端或者服务端
    int role;

    // 接收报文状态
    unsigned int state;
    // 当前处理进度
    unsigned int recv_offset;
    // 当前数据包起始偏移
    unsigned int msg_offset;

    // 指令
    unsigned int op_code;
    unsigned int op_type:3;
    struct {
        unsigned char *s;
        unsigned long long int l;
    } op_value;
    
    unsigned int payload_length;
    unsigned char *payload;
    
    // 发送报文队列
    wbt_bmtp2_msg_list_t send_list;
    
    // 连接状态
    unsigned int is_conn:1;
    // 发送窗口，用于流量控制
    unsigned int window;
    // 最后一次分配的流标识符
    wbt_mq_id stream_id;
} wbt_bmtp2_t;

wbt_status wbt_bmtp2_init();
wbt_status wbt_bmtp2_exit();

wbt_status wbt_bmtp2_on_conn(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_recv(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_send(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_close(wbt_event_t *ev);

wbt_status wbt_bmtp2_on_handler(wbt_event_t *ev);

wbt_status wbt_bmtp2_param_parser(wbt_event_t *ev, wbt_status (*callback)(wbt_event_t *ev, wbt_bmtp2_param_t *p));

wbt_status wbt_bmtp2_on_connect(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_connack(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_pub(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_puback(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_sub(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_suback(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_ping(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_disconn(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_window(wbt_event_t *ev);
wbt_status wbt_bmtp2_on_sync(wbt_event_t *ev);

wbt_status wbt_bmtp2_notify(wbt_event_t *ev);

wbt_status wbt_bmtp2_send_conn(wbt_event_t *ev, wbt_str_t *auth);
wbt_status wbt_bmtp2_send_connack(wbt_event_t *ev, unsigned char status);
wbt_status wbt_bmtp2_send_pub(wbt_event_t *ev, wbt_msg_t *msg);
wbt_status wbt_bmtp2_send_puback(wbt_event_t *ev, wbt_mq_id stream_id, unsigned char status);
wbt_status wbt_bmtp2_send_sub(wbt_event_t *ev, wbt_mq_id channel_id);
wbt_status wbt_bmtp2_send_suback(wbt_event_t *ev, wbt_mq_id channel_id, unsigned char status);
wbt_status wbt_bmtp2_send_ping(wbt_event_t *ev);
wbt_status wbt_bmtp2_send_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp2_send_disconn(wbt_event_t *ev);
wbt_status wbt_bmtp2_send_window(wbt_event_t *ev, unsigned int size);
wbt_status wbt_bmtp2_send_sync(wbt_event_t *ev, wbt_mq_id msg_id);

wbt_status wbt_bmtp2_send(wbt_event_t *ev, wbt_bmtp2_msg_list_t *node);

wbt_status wbt_bmtp2_append_opcode(wbt_bmtp2_msg_list_t *node, unsigned int op_code, unsigned char op_type, unsigned long long int l);
wbt_status wbt_bmtp2_append_param(wbt_bmtp2_msg_list_t *node, unsigned char key, unsigned char key_type, unsigned long long int l, unsigned char *s);
wbt_status wbt_bmtp2_append_payload(wbt_bmtp2_msg_list_t *node, wbt_msg_t *msg);


#ifdef __cplusplus
}
#endif

#endif /* WBT_BMTP2_H */

