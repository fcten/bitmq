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
                bmtp->state = STATE_RECV_HEADER;
                break;
            case STATE_RECV_HEADER:
                if( bmtp->recv_offset + 1 > ev->buff_len ) {
                    return WBT_OK;
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
                
                bmtp->state = STATE_RECV_HEADER;
                break;
            default:
                // 无法识别的状态
                return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_bmtp_on_send(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    if( bmtp->resp_length > bmtp->send_length ) {
        int n = bmtp->resp_length - bmtp->send_length; 
        ssize_t nwrite = wbt_send(ev, bmtp->resp + bmtp->send_length, n);

        if (nwrite <= 0) {
            if( errno == EAGAIN ) {
                return WBT_OK;
            } else {
                return WBT_ERROR;
            }
        }

        bmtp->send_length += nwrite;
        
        if( bmtp->send_length >= 4096 ) {
            wbt_memcpy(bmtp->resp, bmtp->resp + bmtp->send_length, bmtp->resp_length - bmtp->send_length);
            bmtp->resp = wbt_realloc(bmtp->resp, bmtp->resp_length - bmtp->send_length);
            bmtp->resp_length -= bmtp->send_length;
            bmtp->send_length = 0;
        }

        if( bmtp->resp_length > bmtp->send_length ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }

    if( bmtp->is_exit ) {
        wbt_on_close(ev);
    } else {
        ev->events = EPOLLIN | EPOLLET;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_close(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    wbt_free(bmtp->resp);
    
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
    wbt_log_debug("new pub\n");
    
    wbt_bmtp_t *bmtp = ev->data;
    
    if( wbt_bmtp_qos(bmtp->header) == 0 ) {
        return WBT_OK;
    } else {
        return wbt_bmtp_send_puback(ev);
    }
}

wbt_status wbt_bmtp_on_puback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;


    
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

    ev->events = EPOLLOUT | EPOLLIN | EPOLLET;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.event_timeout;

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
    if( sid < 0 ) {
        return WBT_ERROR;
    }
    
    char buf[4] = {BMTP_PUB + dup + qos, sid, len >> 8, len};
    if( wbt_bmtp_send(ev, buf, 4) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    return wbt_bmtp_send(ev, data, len & 0xFFFF);
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
    
    if( ev->events != EPOLLOUT | EPOLLIN | EPOLLET ) {
        ev->events = EPOLLOUT | EPOLLIN | EPOLLET;
        ev->timer.timeout = wbt_cur_mtime + wbt_conf.event_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    }

    void *tmp = wbt_realloc( bmtp->resp, bmtp->resp_length + len );
    if( !tmp ) {
        return WBT_ERROR;
    }
    bmtp->resp = tmp;
    wbt_memcpy(bmtp->resp + bmtp->resp_length, buf, len);
    bmtp->resp_length += len;
    
    return WBT_OK;
}