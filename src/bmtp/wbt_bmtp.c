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
#include "wbt_bmtp.h"

enum {
    STATE_CONNECTED,
    STATE_RECV_HEADER,
    STATE_PARSE_HEADER,
    STATE_RECV_FLAG,
    STATE_PARSE_FLAG,
    STATE_RECV_PAYLOAD,
    STATE_PARSE_PAYLOAD,
    STATE_PARSE_END,
    STATE_RECV_END,
    STATE_ERROR
};

wbt_status wbt_bmtp_init(wbt_event_t *ev);
wbt_status wbt_bmtp_exit(wbt_event_t *ev);
wbt_status wbt_bmtp_on_conn(wbt_event_t *ev);
wbt_status wbt_bmtp_on_recv(wbt_event_t *ev);
wbt_status wbt_bmtp_on_send(wbt_event_t *ev);
wbt_status wbt_bmtp_on_close(wbt_event_t *ev);

wbt_status wbt_bmtp_parse_conn(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_connack(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_pub(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_puback(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_pubrel(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_pubend(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_sub(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_suback(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_unsub(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_unsuback(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_ping(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_pingack(wbt_event_t *ev);
wbt_status wbt_bmtp_parse_disconn(wbt_event_t *ev);

wbt_module_t wbt_module_bmtp = {
    wbt_string("bmtp"),
    wbt_bmtp_init,
    wbt_bmtp_exit/*,
    wbt_bmtp_on_conn,
    wbt_bmtp_on_recv,
    wbt_bmtp_on_send,
    wbt_bmtp_on_close*/
};

wbt_status wbt_bmtp_init(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_exit(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_bmtp_on_conn(wbt_event_t *ev) {
    
}

wbt_status wbt_bmtp_on_recv(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;
    
    ssize_t nread;
    
    while(bmtp->state != STATE_RECV_END) {
        switch(bmtp->state) {
            case STATE_CONNECTED:
                bmtp->state = STATE_RECV_HEADER;
                break;
            case STATE_RECV_HEADER:
                nread = wbt_recv(ev, &bmtp->header, 1);
                if(nread <= 0) {
                    if(errno == EAGAIN) {
                        return WBT_OK;
                    } else {
                        bmtp->state = STATE_ERROR;
                    }
                } else {
                    bmtp->state = STATE_PARSE_HEADER;
                }
                break;
            case STATE_PARSE_HEADER:
                switch(wbt_bmtp_cmd(bmtp->header)) {
                    case BMTP_CONN:
                        if( wbt_bmtp_parse_conn(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_CONNACK:
                        if( wbt_bmtp_parse_connack(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_PUB:
                        if( wbt_bmtp_parse_pub(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_PUBACK:
                        if( wbt_bmtp_parse_puback(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_PUBREL:
                        if( wbt_bmtp_parse_pubrel(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_PUBEND:
                        if( wbt_bmtp_parse_pubend(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_SUB:
                        if( wbt_bmtp_parse_sub(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_SUBACK:
                        if( wbt_bmtp_parse_suback(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_UNSUB:
                        if( wbt_bmtp_parse_unsub(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_UNSUBACK:
                        if( wbt_bmtp_parse_unsuback(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_PING:
                        if( wbt_bmtp_parse_ping(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_PINGACK:
                        if( wbt_bmtp_parse_pingack(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    case BMTP_DISCONN:
                        if( wbt_bmtp_parse_disconn(ev) == WBT_AGAIN ) {
                            return WBT_OK;
                        }
                        break;
                    default:
                        // 无法识别的指令
                        return WBT_ERROR;
                }
                break;
            case STATE_PARSE_END:
                bmtp->state = STATE_RECV_END;
                break;
            default:
                // 无法识别的状态
                return WBT_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_on_send(wbt_event_t *ev) {
    
}

wbt_status wbt_bmtp_on_close(wbt_event_t *ev) {
    
}

wbt_status wbt_bmtp_parse_conn(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    ssize_t nread;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                if( wbt_bmtp_version(bmtp->header) == BMTP_VERSION ) {
                    bmtp->state = STATE_RECV_FLAG;
                } else {
                    bmtp->state = STATE_ERROR;
                }
                break;
            case STATE_RECV_FLAG:
                nread = wbt_recv(ev, &bmtp->flag, 1);
                if(nread <= 0) {
                    if(errno == EAGAIN) {
                        return WBT_AGAIN;
                    } else {
                        bmtp->state = STATE_ERROR;
                    }
                } else {
                    bmtp->state = STATE_PARSE_FLAG;
                }
                break;
            case STATE_PARSE_FLAG:
                bmtp->payload_len = 0;
                if(wbt_bmtp_auth(bmtp->flag)) {
                    bmtp->payload_len += 16;
                }
                
                bmtp->state = STATE_RECV_PAYLOAD;
                break;
            case STATE_RECV_PAYLOAD:
                if( bmtp->payload_len > 0 ) {
                    nread = wbt_recv(ev, &bmtp->flag, 1);
                    if(nread <= 0) {
                        if(errno == EAGAIN) {
                            return WBT_AGAIN;
                        } else {
                            bmtp->state = STATE_ERROR;
                        }
                    } else {
                        bmtp->state = STATE_PARSE_PAYLOAD;
                    }
                } else {
                    bmtp->state = STATE_PARSE_END;
                }
                break;
            case STATE_PARSE_PAYLOAD:
                // TODO 身份验证
                bmtp->state = STATE_PARSE_END;
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_connack(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                if( wbt_bmtp_status(bmtp->header) == 0 ) {
                    bmtp->state = STATE_PARSE_END;
                } else {
                    // TODO 记录失败原因
                    bmtp->state = STATE_ERROR;
                }
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_pub(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_puback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_pubrel(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_pubend(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_sub(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_suback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_unsub(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_unsuback(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_ping(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_pingack(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}

wbt_status wbt_bmtp_parse_disconn(wbt_event_t *ev) {
    wbt_bmtp_t *bmtp = ev->data;

    while(bmtp->state != STATE_PARSE_END && bmtp->state != STATE_ERROR) {
        switch(bmtp->state) {
            case STATE_PARSE_HEADER:
                // 直接关闭连接
                bmtp->state = STATE_ERROR;
                break;
            default:
                bmtp->state = STATE_ERROR;
        }
    }
    
    return WBT_OK;
}
