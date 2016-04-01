/* 
 * File:   wbt_bmtp.c
 * Author: fcten
 *
 * Created on 2016年2月26日, 下午4:42
 */

#include "../webit.h"
#include "../common/wbt_event.h"
#include "../common/wbt_module.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_config.h"
#include "../common/wbt_log.h"
#include "wbt_bmtp.h"
#include "wbt_bmtp_sid.h"

enum {
    STATE_CONNECTED,
    STATE_RECV_HEADER,
    STATE_RECV_SID,
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
wbt_status wbt_bmtp_on_ping(wbt_event_t *ev);
wbt_status wbt_bmtp_on_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp_on_disconn(wbt_event_t *ev);

wbt_status wbt_bmtp_send_conn(wbt_event_t *ev);
wbt_status wbt_bmtp_send_connack(wbt_event_t *ev, unsigned char status);
wbt_status wbt_bmtp_send_pub(wbt_event_t *ev, int dup, int qos, char *buf, unsigned int len);
wbt_status wbt_bmtp_send_puback(wbt_event_t *ev);
wbt_status wbt_bmtp_send_ping(wbt_event_t *ev);
wbt_status wbt_bmtp_send_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp_send_disconn(wbt_event_t *ev);
wbt_status wbt_bmtp_send(wbt_event_t *ev, char *buf, int len);

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
                            bmtp->payload_length = 0;
                            bmtp->state = STATE_RECV_PAYLOAD;
                            break;
                        case BMTP_CONN:
                            bmtp->payload_length = 4;
                            bmtp->state = STATE_RECV_PAYLOAD;
                            break;
                        case BMTP_PUB:
                            if( wbt_bmtp_qos(bmtp->header) == 0 ) {
                                bmtp->state = STATE_RECV_PAYLOAD;
                            } else {
                                bmtp->state = STATE_RECV_SID;
                            }
                            break;
                        case BMTP_PUBACK:
                            bmtp->state = STATE_RECV_SID;
                            break;
                        default:
                            return WBT_ERROR;
                    }
                }
                break;
            case STATE_RECV_SID:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
                }

                switch(wbt_bmtp_cmd(bmtp->header)) {
                    case BMTP_PUB:
                        bmtp->sid = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
                        bmtp->state = STATE_RECV_PAYLOAD_LENGTH;
                        break;
                    case BMTP_PUBACK:
                        bmtp->sid = ((unsigned char *)ev->buff)[bmtp->recv_offset ++];
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
                
                bmtp->payload = ev->buff + bmtp->recv_offset;
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
                    wbt_memcpy(ev->buff, ev->buff + bmtp->recv_offset, ev->buff_len - bmtp->recv_offset);
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
    ev->events |= EPOLLIN;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;
    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    return WBT_OK;
}

wbt_status wbt_bmtp_on_send(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while (!wbt_list_empty(&bmtp->send_queue.head)) {
        wbt_bmtp_msg_t *msg_node = wbt_list_first_entry(&bmtp->send_queue.head, wbt_bmtp_msg_t, head);
        int ret = wbt_send(ev, (char *)msg_node->msg + msg_node->offset, msg_node->len - msg_node->offset);
        if (ret <= 0) {
            if( errno == EAGAIN ) {
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
        ev->events &= ~EPOLLOUT;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_close(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    // TODO 释放队列
    
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
    } else {
        bmtp->is_conn = 1;
        return wbt_bmtp_send_connack(ev, 0x0);
    }
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
        return WBT_OK;
    } else {
        wbt_bmtp_send_puback(ev);
        return wbt_bmtp_send_pub(ev, 0, 1, "123", 3);
        return wbt_bmtp_send_puback(ev);
    }
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
    }
    else {
        msg_node = wbt_list_first_entry(&bmtp->wait_queue.head, wbt_bmtp_msg_t, head);
        msg_node->msg[1] = bmtp->sid;
        wbt_list_del(&msg_node->head);
        wbt_list_add_tail(&msg_node->head, &bmtp->send_queue.head);

        ev->events |= EPOLLOUT;
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

    ev->events |= EPOLLOUT;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    bmtp->is_exit = 1;
    
    return WBT_OK;
}

wbt_status wbt_bmtp_send_conn(wbt_event_t *ev) {
    char buf[5] = {BMTP_CONN + BMTP_VERSION, 'B', 'M', 'T', 'P'};
    
    return wbt_bmtp_send(ev, buf, 1);
}

wbt_status wbt_bmtp_send_connack(wbt_event_t *ev, unsigned char status) {
    char buf[1] = {BMTP_CONNACK + status};
    
    return wbt_bmtp_send(ev, buf, 1);
}

wbt_status wbt_bmtp_send_pub(wbt_event_t *ev, int dup, int qos, char *data, unsigned int len) {
    int sid = wbt_bmtp_sid_alloc(ev->data);
    
    char buf[64 * 1024] = {BMTP_PUB + dup + qos, sid, len >> 8, len};
    if( len > 64 * 1024 - 4 ) {
        memcpy(buf+4, data, 64 * 1024 - 4);
        return wbt_bmtp_send(ev, buf, 64 * 1024);
    } else {
        memcpy(buf+4, data, len);
        return wbt_bmtp_send(ev, buf, len + 4);
    }
}

wbt_status wbt_bmtp_send_puback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    char buf[2] = {BMTP_PUBACK, bmtp->sid};
    
    return wbt_bmtp_send(ev, buf, sizeof(buf));
}

wbt_status wbt_bmtp_send_ping(wbt_event_t *ev) {
    char buf[1] = {BMTP_PING};
    
    return wbt_bmtp_send(ev, buf, 1);
}

wbt_status wbt_bmtp_send_pingack(wbt_event_t *ev) {
    char buf[1] = {BMTP_PINGACK};
    
    return wbt_bmtp_send(ev, buf, sizeof(buf));
}

wbt_status wbt_bmtp_send_disconn(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    bmtp->is_exit = 1;

    char buf[1] = {BMTP_DISCONN};
    
    return wbt_bmtp_send(ev, buf, sizeof(buf));
}

wbt_status wbt_bmtp_send(wbt_event_t *ev, char *buf, int len) {
    wbt_bmtp_t *bmtp = ev->data;

    wbt_bmtp_msg_t *msg_node = (wbt_bmtp_msg_t *)wbt_calloc(sizeof(wbt_bmtp_msg_t));
    if (!msg_node) {
        return WBT_ERROR;
    }

    msg_node->msg =(unsigned char *)wbt_malloc(len);
    if (!msg_node->msg) {
        wbt_free(msg_node);
        return WBT_ERROR;
    }
    msg_node->len = len;
    memcpy(msg_node->msg, buf, len);
    
    if (wbt_bmtp_cmd(buf[0]) == BMTP_PUB && buf[1] == 0) {
        wbt_list_add_tail(&msg_node->head, &bmtp->wait_queue.head);
    }
    else {
        wbt_list_add_tail(&msg_node->head, &bmtp->send_queue.head);

        ev->events |= EPOLLOUT;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}