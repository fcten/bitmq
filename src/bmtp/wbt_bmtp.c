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
wbt_status wbt_bmtp_send_connack(wbt_event_t *ev);
wbt_status wbt_bmtp_send_pub(wbt_event_t *ev);
wbt_status wbt_bmtp_send_puback(wbt_event_t *ev);
wbt_status wbt_bmtp_send_ping(wbt_event_t *ev);
wbt_status wbt_bmtp_send_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp_send_disconn(wbt_event_t *ev);

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
    wbt_bmtp_t *bmtp = ev->data;
    
    wbt_log_debug("new conn\n");

    ev->on_send = wbt_on_send;
    ev->events = EPOLLOUT | EPOLLIN | EPOLLET;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.event_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    void *tmp = wbt_realloc( bmtp->resp, bmtp->resp_length + 1 );
    if( !tmp ) {
        return WBT_ERROR;
    }
    bmtp->resp = tmp;

    if( wbt_bmtp_version(bmtp->header) != BMTP_VERSION ||
            bmtp->payload[0] != 'B' ||
            bmtp->payload[1] != 'M' ||
            bmtp->payload[2] != 'T' ||
            bmtp->payload[3] != 'P') {
        bmtp->resp[bmtp->resp_length ++] = BMTP_CONNACK & 0xF1;
        bmtp->is_exit = 1;
    } else {
        bmtp->resp[bmtp->resp_length ++] = BMTP_CONNACK & 0xF0;
        bmtp->is_conn = 1;
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_connack(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;


    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_pub(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    wbt_log_debug("new pub\n");
    
    if( wbt_bmtp_qos(bmtp->header) == 0 ) {
        return WBT_OK;
    }

    ev->events = EPOLLOUT | EPOLLIN | EPOLLET;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.event_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    void *tmp = wbt_realloc( bmtp->resp, bmtp->resp_length + 2 );
    if( !tmp ) {
        return WBT_ERROR;
    }
    bmtp->resp = tmp;

    bmtp->resp[bmtp->resp_length ++] = BMTP_PUBACK & 0xF0;
    bmtp->resp[bmtp->resp_length ++] = bmtp->sid;
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_puback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;


    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_ping(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    wbt_log_debug("new ping\n");

    ev->events = EPOLLOUT | EPOLLIN | EPOLLET;
    ev->timer.timeout = wbt_cur_mtime + wbt_conf.event_timeout;

    if(wbt_event_mod(ev) != WBT_OK) {
        return WBT_ERROR;
    }

    void *tmp = wbt_realloc( bmtp->resp, bmtp->resp_length + 1 );
    if( !tmp ) {
        return WBT_ERROR;
    }
    bmtp->resp = tmp;

    bmtp->resp[bmtp->resp_length ++] = BMTP_PINGACK & 0xF0;
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_pingack(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    
    
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
    wbt_bmtp_t *bmtp = ev->data;


    
    return WBT_OK;
}

wbt_status wbt_bmtp_send_connack(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_send_pub(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_send_puback(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_send_ping(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_send_pingack(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_send_disconn(wbt_event_t *ev) {
    return WBT_OK;
}