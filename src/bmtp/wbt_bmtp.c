/* 
 * File:   wbt_bmtp.c
 * Author: fcten
 *
 * Created on 2016年2月26日, 下午4:42
 */

#include "../webit.h"
#include "../event/wbt_event.h"
#include "../common/wbt_module.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_config.h"
#include "../common/wbt_log.h"
#include "../json/wbt_json.h"
#include "../mq/wbt_mq.h"
#include "../mq/wbt_mq_msg.h"
#include "wbt_bmtp.h"
#include "wbt_bmtp_sid.h"

enum {
    STATE_CONNECTED,
    STATE_RECV_HEADER,
    STATE_RECV_SID,
    STATE_RECV_CID,
    STATE_RECV_PAYLOAD_LENGTH,
    STATE_RECV_PAYLOAD
};

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

wbt_module_t wbt_module_bmtp = {
    wbt_string("bmtp"),
    wbt_bmtp_init,
    wbt_bmtp_exit,
    wbt_bmtp_on_conn,
    wbt_bmtp_on_recv,
    wbt_bmtp_on_send,
    wbt_bmtp_on_close
};

wbt_status wbt_bmtp_init() {


    return WBT_OK;
}

wbt_status wbt_bmtp_exit() {
    return WBT_OK;
}

wbt_status wbt_bmtp_on_conn(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    ev->data = wbt_calloc( sizeof(wbt_bmtp_t) );
    if( ev->data == NULL ) {
        return WBT_ERROR;
    }

    wbt_bmtp_t *bmtp = ev->data;
    
    bmtp->usable_sids = 255;

    wbt_list_init(&bmtp->wait_queue.head);
    wbt_list_init(&bmtp->send_queue.head);
    wbt_list_init(&bmtp->ack_queue.head);

    bmtp->state = STATE_CONNECTED;
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_recv(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    wbt_bmtp_t *bmtp = ev->data;
    
    if( bmtp->is_exit ) {
        return WBT_OK;
    }
    
    while(!bmtp->is_exit) {
        switch(bmtp->state) {
            case STATE_CONNECTED:
                bmtp->sid = 0;
                bmtp->payload = NULL;
                bmtp->payload_length = 0;
                bmtp->state = STATE_RECV_HEADER;
                break;
            case STATE_RECV_HEADER:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    goto waiting;
                }

                bmtp->header = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];

                if( (!bmtp->is_conn && wbt_bmtp_cmd(bmtp->header) != BMTP_CONN)
                        || (bmtp->is_conn && wbt_bmtp_cmd(bmtp->header) == BMTP_CONN)) {
                    return WBT_ERROR;
                } else {
                    switch(wbt_bmtp_cmd(bmtp->header)) {
                        case BMTP_CONNACK:
                        case BMTP_PING:
                        case BMTP_PINGACK:
                        case BMTP_DISCONN:
                            bmtp->state = STATE_RECV_PAYLOAD;
                            break;
                        case BMTP_CONN:
                            bmtp->payload_length = 4;
                            bmtp->state = STATE_RECV_PAYLOAD;
                            break;
                        case BMTP_PUB:
                            if( wbt_bmtp_qos(bmtp->header) == 0 ) {
                                bmtp->state = STATE_RECV_PAYLOAD_LENGTH;
                            } else {
                                bmtp->state = STATE_RECV_SID;
                            }
                            break;
                        case BMTP_PUBACK:
                            bmtp->state = STATE_RECV_SID;
                            break;
                        case BMTP_SUB:
                            bmtp->state = STATE_RECV_CID;
                            break;
                        default:
                            return WBT_ERROR;
                    }
                }
                break;
            case STATE_RECV_CID:
                if( bmtp->recv_offset + 4 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                bmtp->cid  = ((unsigned char *)ev->buff)[bmtp->recv_offset ++] << 24;
                bmtp->cid += ((unsigned char *)ev->buff)[bmtp->recv_offset ++] << 16;
                bmtp->cid += ((unsigned char *)ev->buff)[bmtp->recv_offset ++] << 8;
                bmtp->cid += ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                bmtp->state = STATE_RECV_PAYLOAD;
                break;
            case STATE_RECV_SID:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }
                
                bmtp->sid = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];

                switch(wbt_bmtp_cmd(bmtp->header)) {
                    case BMTP_PUB:
                        bmtp->state = STATE_RECV_PAYLOAD_LENGTH;
                        break;
                    case BMTP_PUBACK:
                        bmtp->state = STATE_RECV_PAYLOAD;
                        break;
                    default:
                        return WBT_ERROR;
                }
                break;
            case STATE_RECV_PAYLOAD_LENGTH:
                if( bmtp->recv_offset + 2 > ev->buff_len ) {
                    return WBT_OK;
                }

                bmtp->payload_length = ((unsigned char *)ev->buff)[bmtp->recv_offset ++] << 8;
                bmtp->payload_length += ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                bmtp->state = STATE_RECV_PAYLOAD;
                break;
            case STATE_RECV_PAYLOAD:
                if( bmtp->recv_offset + bmtp->payload_length > ev->buff_len ) {
                    return WBT_OK;
                }
                
                bmtp->payload = (unsigned char *)ev->buff + bmtp->recv_offset;
                bmtp->recv_offset += bmtp->payload_length;

                switch(wbt_bmtp_cmd(bmtp->header)) {
                    case BMTP_CONN:
                        if( wbt_bmtp_on_connect(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_CONNACK:
                        if( wbt_bmtp_on_connack(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_PUB:
                        if( wbt_bmtp_on_pub(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_PUBACK:
                        if( wbt_bmtp_on_puback(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_SUB:
                        if( wbt_bmtp_on_sub(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_PING:
                        if( wbt_bmtp_on_ping(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_PINGACK:
                        if( wbt_bmtp_on_pingack(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    case BMTP_DISCONN:
                        if( wbt_bmtp_on_disconn(ev) != WBT_OK ) {
                            return WBT_ERROR;
                        }
                        break;
                    default:
                        return WBT_ERROR;
                }

                if( bmtp->recv_offset >= 4096 ) {
                    wbt_memcpy(ev->buff, (unsigned char *)ev->buff + bmtp->recv_offset, ev->buff_len - bmtp->recv_offset);
                    ev->buff = wbt_realloc(ev->buff, ev->buff_len - bmtp->recv_offset);
                    ev->buff_len -= bmtp->recv_offset;
                    bmtp->recv_offset = 0;
                }
                
                bmtp->state = STATE_CONNECTED;
                break;
            default:
                // 无法识别的状态
                return WBT_ERROR;
        }
    }
    
waiting:
    ev->events |= WBT_EV_READ;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;
    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_bmtp_on_send(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    wbt_bmtp_t *bmtp = ev->data;

    while (!wbt_list_empty(&bmtp->send_queue.head)) {
        wbt_bmtp_msg_t *msg_node = wbt_list_first_entry(&bmtp->send_queue.head, wbt_bmtp_msg_t, head);
        int ret = wbt_send(ev, (char *)msg_node->msg + msg_node->offset, msg_node->len - msg_node->offset);
        if (ret <= 0) {
            if( wbt_socket_errno == WBT_EAGAIN ) {
                return WBT_OK;
            } else {
                return WBT_ERROR;
            }
        } else {
            if (ret + msg_node->offset >= msg_node->len) {
                wbt_list_del(&msg_node->head);
                if (wbt_bmtp_cmd(msg_node->msg[0]) == BMTP_PUB && wbt_bmtp_qos(msg_node->msg[0]) > 0) {
                    wbt_list_add_tail(&msg_node->head, &bmtp->ack_queue.head);
                }
                else {
                    wbt_free(msg_node->msg);
                    wbt_free(msg_node);
                }
            } else {
                msg_node->offset += ret;
            }
        }
    }
    
    if( bmtp->is_exit ) {
        wbt_on_close(ev);
    } else {
        ev->events &= ~WBT_EV_WRITE;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_close(wbt_event_t *ev) {
    if( ev->protocol != WBT_PROTOCOL_BMTP ) {
        return WBT_OK;
    }

    wbt_bmtp_t *bmtp = ev->data;
    
    // TODO 如果不保存会话，则释放队列
    wbt_bmtp_msg_t *msg_node;
    
    while(!wbt_list_empty(&bmtp->wait_queue.head)) {
        msg_node = wbt_list_first_entry(&bmtp->wait_queue.head, wbt_bmtp_msg_t, head);
        wbt_list_del(&msg_node->head);
        wbt_free( msg_node->msg );
        wbt_free( msg_node );
    }

    while(!wbt_list_empty(&bmtp->send_queue.head)) {
        msg_node = wbt_list_first_entry(&bmtp->send_queue.head, wbt_bmtp_msg_t, head);
        wbt_list_del(&msg_node->head);
        wbt_free( msg_node->msg );
        wbt_free( msg_node );
    }

    while(!wbt_list_empty(&bmtp->ack_queue.head)) {
        msg_node = wbt_list_first_entry(&bmtp->ack_queue.head, wbt_bmtp_msg_t, head);
        wbt_list_del(&msg_node->head);
        wbt_free( msg_node->msg );
        wbt_free( msg_node );
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_connect(wbt_event_t *ev) {
    wbt_log_debug("new conn\n");

    wbt_bmtp_t *bmtp = ev->data;

    if( wbt_bmtp_version(bmtp->header) != BMTP_VERSION ||
            bmtp->payload[0] != 'B' ||
            bmtp->payload[1] != 'M' ||
            bmtp->payload[2] != 'T' ||
            bmtp->payload[3] != 'P') {
        bmtp->is_exit = 1;
        return wbt_bmtp_send_connack(ev, 0x1);
    }

    if( wbt_mq_login(ev) != WBT_OK ) {
        return wbt_bmtp_send_connack(ev, 0x2);
    }
    
    wbt_mq_set_send_cb(ev, wbt_bmtp_send_pub);
    wbt_mq_set_is_ready_cb(ev, wbt_bmtp_is_ready);
    
    bmtp->is_conn = 1;
    if( wbt_bmtp_send_connack(ev, 0x0) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_connack(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    bmtp->is_conn = 1;
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_pub(wbt_event_t *ev) {
    //wbt_log_debug("new pub\n");
    
    wbt_bmtp_t *bmtp = ev->data;

    if( wbt_bmtp_qos(bmtp->header) == 0 ) {
        wbt_mq_push(ev, bmtp->payload, bmtp->payload_length);
        return WBT_OK;
    } else {
        if( wbt_mq_push(ev, bmtp->payload, bmtp->payload_length) != WBT_OK ) {
            // TODO 需要返回更详细的错误原因
            return wbt_bmtp_send_puback(ev, 0x2);
        } else {
            return wbt_bmtp_send_puback(ev, 0x0);
        }
    }
}

wbt_status wbt_bmtp_on_sub(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    // 在所有想要订阅的频道的 subscriber_list 中添加该订阅者
    if( wbt_mq_subscribe(ev, bmtp->cid) != WBT_OK ) {
        return wbt_bmtp_send_suback(ev, 0x1);
    }
    
    wbt_str_t resp;
    resp.len = 1024 * 64;
    resp.str = wbt_malloc( resp.len );
    if( resp.str == NULL ) {
        bmtp->is_exit = 1;
        return wbt_bmtp_send_suback(ev, 0x2);
    }
    
    if( wbt_bmtp_send_suback(ev, 0x0) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    // BMTP 协议允许一次性推送 255 条消息
    // 但是为了控制内存消耗，这里还限制了消息总长度
    // TODO qos=0 的离线消息数量过多时无法推送完整
    wbt_msg_t *msg;
    int total = 0;
    while( wbt_mq_pull(ev, &msg) == WBT_OK && msg) {
        json_object_t *obj = wbt_mq_msg_print(msg);

        char *p = resp.str;
        size_t l = 1024 * 64;
        json_print(obj, &p, &l);
        resp.len = 1024 * 64 - l;
        
        json_delete_object(obj);
  
        // TODO 如果这里 send 失败了
        wbt_bmtp_send_pub(ev, resp.str, resp.len, msg->qos, 0 );
        
        total += resp.len;
        if( total >= 1024 * 64 ) {
            break;
        }
    }
    
    wbt_free(resp.str);

    return WBT_OK;
}

wbt_status wbt_bmtp_on_puback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    //wbt_log_debug("recv puback sid %d\n", bmtp->sid);

    // 搜索 ack_queue
    wbt_bmtp_msg_t *msg_node;
    wbt_list_for_each_entry(msg_node, wbt_bmtp_msg_t, &bmtp->ack_queue.head, head) {
        if (msg_node->msg[1] == bmtp->sid) {
            wbt_list_del(&msg_node->head);
            wbt_free(msg_node->msg);
            wbt_free(msg_node);
            break;
        }
    }

    if (wbt_list_empty(&bmtp->wait_queue.head)) {
        wbt_bmtp_sid_free(bmtp, bmtp->sid);
        
        wbt_str_t resp;
        resp.len = 1024 * 64;
        resp.str = wbt_malloc( resp.len );
        if( resp.str == NULL ) {
            return wbt_bmtp_on_disconn(ev);
        }

        wbt_msg_t *msg;
        if( wbt_mq_pull(ev, &msg) == WBT_OK && msg) {
            json_object_t *obj = wbt_mq_msg_print(msg);

            char *p = resp.str;
            size_t l = 1024 * 64;
            json_print(obj, &p, &l);
            resp.len = 1024 * 64 - l;
            
            json_delete_object(obj);

            wbt_bmtp_send_pub(ev, resp.str, resp.len, msg->qos, 0 );
        }
        
        wbt_free(resp.str);
    }
    else {
        msg_node = wbt_list_first_entry(&bmtp->wait_queue.head, wbt_bmtp_msg_t, head);
        msg_node->msg[1] = bmtp->sid;
        wbt_list_del(&msg_node->head);
        wbt_list_add_tail(&msg_node->head, &bmtp->send_queue.head);

        ev->events |= WBT_EV_WRITE;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_ping(wbt_event_t *ev) {
    wbt_log_debug("new ping\n");
    
    return wbt_bmtp_send_pingack(ev);
}

wbt_status wbt_bmtp_on_pingack(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_on_disconn(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    ev->events |= WBT_EV_WRITE;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    bmtp->is_exit = 1;
    
    return WBT_OK;
}

wbt_status wbt_bmtp_send_conn(wbt_event_t *ev) {
    char buf[] = {BMTP_CONN + BMTP_VERSION, 'B', 'M', 'T', 'P'};
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send_connack(wbt_event_t *ev, unsigned char status) {
    char buf[] = {BMTP_CONNACK + status};
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send_pub(wbt_event_t *ev, char *data, unsigned int len, int qos, int dup) {
    if( qos > 0 ) {
        int sid = wbt_bmtp_sid_alloc(ev->data);

        if( len > 64 * 1024 - 4 ) {
            len = 64 * 1024 - 4;
        }

        char *buf = wbt_malloc(len+4);
        buf[0] = BMTP_PUB + dup + qos;
        buf[1] = sid;
        buf[2] = len >> 8;
        buf[3] = len;
        wbt_memcpy(buf+4, data, len);
        return wbt_bmtp_send(ev, buf, len+4);
    } else {
        if( len > 64 * 1024 - 3 ) {
            len = 64 * 1024 - 3;
        }

        char *buf = wbt_malloc(len+3);
        buf[0] = BMTP_PUB + dup + qos;
        buf[1] = len >> 8;
        buf[2] = len;
        wbt_memcpy(buf+3, data, len);
        return wbt_bmtp_send(ev, buf, len+3);
    }
}

wbt_status wbt_bmtp_send_puback(wbt_event_t *ev, unsigned char status) {
    wbt_bmtp_t *bmtp = ev->data;
    
    char buf[] = {BMTP_PUBACK + status, bmtp->sid};
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send_suback(wbt_event_t *ev, unsigned char status) {
    wbt_bmtp_t *bmtp = ev->data;
    
	char buf[] = { BMTP_SUBACK + status, (char)(bmtp->cid << 24), (char)(bmtp->cid << 16), (char)(bmtp->cid << 8), (char)bmtp->cid };
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send_ping(wbt_event_t *ev) {
    char buf[] = {BMTP_PING};
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send_pingack(wbt_event_t *ev) {
    char buf[] = {BMTP_PINGACK};
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send_disconn(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    bmtp->is_exit = 1;

    char buf[] = {BMTP_DISCONN};
    
    return wbt_bmtp_send(ev, wbt_strdup(buf, sizeof(buf)), sizeof(buf));
}

wbt_status wbt_bmtp_send(wbt_event_t *ev, char *buf, int len) {
    wbt_bmtp_t *bmtp = ev->data;

    wbt_bmtp_msg_t *msg_node = (wbt_bmtp_msg_t *)wbt_calloc(sizeof(wbt_bmtp_msg_t));
    if (!msg_node) {
        return WBT_ERROR;
    }

    msg_node->len = len;
    msg_node->msg = buf;
    
    if (wbt_bmtp_cmd(buf[0]) == BMTP_PUB &&
        wbt_bmtp_qos(buf[0]) > 0 &&
        buf[1] == 0) {
        wbt_list_add_tail(&msg_node->head, &bmtp->wait_queue.head);
        wbt_log_debug( "add to wait queue\n" );
    }
    else {
        wbt_list_add_tail(&msg_node->head, &bmtp->send_queue.head);
        wbt_log_debug( "add to send queue\n" );

        ev->events |= WBT_EV_WRITE;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_is_ready(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    if( wbt_list_empty(&bmtp->wait_queue.head) ) {
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}