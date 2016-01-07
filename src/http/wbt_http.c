 /* 
 * File:   wbt_http.c
 * Author: Fcten
 *
 * Created on 2014年10月24日, 下午3:30
 */

#include <sys/sendfile.h>

#include "../common/wbt_module.h"
#include "../common/wbt_memory.h"
#include "../common/wbt_log.h"
#include "../common/wbt_time.h"
#include "../common/wbt_config.h"
#include "../common/wbt_connection.h"
#include "../common/wbt_ssl.h"
#include "../common/wbt_string.h"
#include "../common/wbt_file.h"
#include "wbt_http.h"

extern wbt_atomic_t wbt_wating_to_exit;

wbt_module_t wbt_module_http1_parser = {
    wbt_string("http-parser"),
    wbt_http_init,
    wbt_http_exit,
    wbt_http_on_conn,
    wbt_http_on_recv,
    wbt_http_on_send,
    wbt_http_on_close
};

wbt_module_t wbt_module_http1_generater = {
    wbt_string("http-generater"),
    NULL,
    NULL,
    NULL,
    wbt_http_generater,
    NULL,
    NULL,
};

wbt_mem_t wbt_file_path;
wbt_mem_t wbt_send_buf;

wbt_status wbt_http_init() {
    /* 初始化 sendbuf 与 file_path */
    // TODO 根据读取的数据长度分配内存
    wbt_malloc(&wbt_send_buf, 10240);
    wbt_malloc(&wbt_file_path, 512);
    
    return WBT_OK;
}

wbt_status wbt_http_exit() {
    return WBT_OK;
}

wbt_status wbt_http_on_conn( wbt_event_t *ev ) {
    if( wbt_malloc(&ev->data, sizeof(wbt_http_t)) != WBT_OK ) {
        return WBT_ERROR;
    }
    wbt_memset(&ev->data, 0);

    wbt_http_t *http = ev->data.ptr;

    http->state = STATE_CONNECTION_ESTABLISHED;
    
    return WBT_OK;
}

wbt_status wbt_http_on_send( wbt_event_t *ev ) {
    int nwrite, n;
    wbt_http_t *http = ev->data.ptr;
    
    if( http->state != STATE_SENDING ) {
        return WBT_OK;
    }
    
    // 由模块自己处理数据发送，是为了避免不必要的内存拷贝
    
    int on = 1;
    /* TODO 发送大文件时，使用 TCP_CORK 关闭 Nagle 算法保证网络利用率 */
    //setsockopt( ev->fd, SOL_TCP, TCP_CORK, &on, sizeof ( on ) );
    /* 测试 keep-alive 性能时，使用 TCP_NODELAY 立即发送小数据，否则会阻塞 40 毫秒 */
    setsockopt( ev->fd, SOL_TCP, TCP_NODELAY, &on, sizeof ( on ) );

    /* 如果存在 response，发送 response */
    if( http->response.len - http->resp_offset > 0 ) {
        n = http->response.len - http->resp_offset; 
        nwrite = wbt_ssl_write(ev, http->response.ptr + http->resp_offset, n);

        if (nwrite == -1) {
            if( wbt_conf.secure ) {
                int err = wbt_ssl_get_error(ev, nwrite);
                switch(err) {
                    case SSL_ERROR_WANT_WRITE:
                        wbt_log_debug("SSL_ERROR_WANT_WRITE");
                        break;
                    case SSL_ERROR_WANT_READ:
                        wbt_log_debug("SSL_ERROR_WANT_READ");
                        wbt_conn_close(ev);
                        break;
                    case SSL_ERROR_SYSCALL:
                        wbt_log_debug("SSL_ERROR_SYSCALL");
                        wbt_conn_close(ev);
                        break;
                    default:
                        wbt_log_debug("%d", err);
                        wbt_conn_close(ev);
                }
            } else {
                if( errno != EAGAIN ) {
                    /* 这里数据发送失败了，但应当返回 WBT_OK。这个函数的返回值
                     * 仅用于判断是否发生了必须重启工作进程的严重错误。
                     * 如果模块需要处理数据发送失败的错误，必须根据 state 在 on_close 回调中处理。
                     */
                    wbt_conn_close(ev);
                }
            }
            
            return WBT_OK;
        }

        http->resp_offset += nwrite;

        wbt_log_debug("%d send, %d remain.", nwrite, http->response.len - http->resp_offset);
        if( http->response.len > http->resp_offset ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }
    
    if( http->status == STATUS_200 && http->file.size > http->file.offset  ) {
        // 在非阻塞模式下，对于大量数据，每次只能发送一部分
        // 需要在未发送完成前继续监听可写事件
        n = http->file.size - http->file.offset;

        if( http->file.ptr != NULL ) {
            // 需要发送的数据已经在内存中
            nwrite = wbt_ssl_write(ev, http->file.ptr + http->file.offset, n);
            if( nwrite >= 0 ) {
                http->file.offset += nwrite;
            }
        } else if( http->file.fd > 0 && wbt_conf.sendfile ) {
            // 需要发送的数据不在内存中，但是指定了需要发送的文件
            nwrite = sendfile( ev->fd, http->file.fd, &http->file.offset, n );
        } else {
            // 不应该出现这种情况
            n = nwrite = 0;
        }

        if (nwrite == -1) {
            if( wbt_conf.secure ) {
                int err = wbt_ssl_get_error(ev, nwrite);
                switch(err) {
                    case SSL_ERROR_WANT_WRITE:
                        wbt_log_debug("SSL_ERROR_WANT_WRITE");
                        break;
                    case SSL_ERROR_WANT_READ:
                        wbt_log_debug("SSL_ERROR_WANT_READ");
                        wbt_conn_close(ev);
                        break;
                    case SSL_ERROR_SYSCALL:
                        wbt_log_debug("SSL_ERROR_SYSCALL");
                        wbt_conn_close(ev);
                        break;
                    default:
                        wbt_conn_close(ev);
                }
            } else {
                if( errno != EAGAIN ) {
                    /* 连接被意外关闭，同上 */
                    wbt_conn_close(ev);
                }
            }
            
            return WBT_OK;
        }

        wbt_log_debug("%d send, %d remain.", nwrite, n - nwrite);
        if( n > nwrite ) {
            /* 尚未发送完，缓冲区满 */
            return WBT_OK;
        }
    }
    
    /* 所有数据发送完毕 */
    http->state = STATE_SEND_COMPLETED;
    
    wbt_str_t http_uri, req_body;
    wbt_offset_to_str(http->uri, http_uri, ev->buff.ptr);
    wbt_offset_to_str(http->body, req_body, ev->buff.ptr);
    wbt_log_add("%.*s %.*s %.*s\n%.*s\n",
        REQUEST_METHOD[http->method].len,
        REQUEST_METHOD[http->method].str,
        http_uri.len,
        http_uri.str,
        3,
        STATUS_CODE[http->status].str,
        req_body.len,
        req_body.str);
    
    /* 如果是 keep-alive 连接，继续等待数据到来 */
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE && !wbt_wating_to_exit ) {
        wbt_http_on_close( ev );
        /* 为下一个连接初始化相关结构 */
        if( wbt_malloc(&ev->data, sizeof(wbt_http_t)) != WBT_OK ) {
            return WBT_ERROR;
        }
        wbt_memset(&ev->data, 0);
        
        http = ev->data.ptr;
        http->state = STATE_SEND_COMPLETED;

        ev->on_recv = wbt_on_recv;
        ev->events = EPOLLIN | EPOLLET;
        ev->timeout = cur_mtime + wbt_conf.keep_alive_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
    } else {
        /* 非 keep-alive 连接，直接关闭 */
        wbt_conn_close(ev);
    }
    
    return WBT_OK;
}

/* 释放 http 结构体中动态分配的内存 */
wbt_status wbt_http_on_close( wbt_event_t *ev ) {
    wbt_http_header_t *header, *next;
    wbt_mem_t tmp;
    wbt_http_t *http = ev->data.ptr;
    
    if( http ) {
    
        /* 关闭已打开的文件（不是真正的关闭，只是声明该文件已经在本次处理中使用完毕） */
        if( http->file.fd > 0 ) {
            // TODO 需要判断 URI 是否以 / 开头
            // TODO 需要和前面的代码整合到一起
            wbt_str_t full_path;
            if( *((char *)ev->buff.ptr + http->uri.start + http->uri.len - 1)\
                    == '/' ) {
                full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
                    http->uri.len,
                    (char *)ev->buff.ptr + http->uri.start,
                    wbt_conf.index.len, wbt_conf.index.str);
            } else {
                full_path = wbt_sprintf(&wbt_file_path, "%.*s",
                    http->uri.len,
                    (char *)ev->buff.ptr + http->uri.start);
            }
            wbt_file_close( &full_path );
        }

        /* 释放动态分配的内存 */
        header = http->headers;
        while( header != NULL ) {
            next = header->next;

            tmp.ptr = header;
            tmp.len = 1;
            wbt_free( &tmp );

            header = next;
        }

        header = http->resp_headers;
        while( header != NULL ) {
            next = header->next;

            tmp.ptr = header;
            tmp.len = 1;
            wbt_free( &tmp );

            header = next;
        }

        /* 释放生成的响应消息头 */
        wbt_free( &http->response );

        /* 释放读入内存的文件内容 */
        if( http->file.ptr ) {
            tmp.ptr = http->file.ptr;
            tmp.len = 1;
            wbt_free( &tmp );
        }
    
    }
    
    /* 释放 http 结构体本身 */
    wbt_free(&ev->data);
    /* 释放事件数据缓存 */
    wbt_free(&ev->buff);

    return WBT_OK;
}

wbt_status wbt_http_check_header_end( wbt_event_t *ev ) {
    wbt_str_t http_req;
    wbt_str_t http_header_end = wbt_string("\r\n\r\n");

    http_req.len = ev->buff.len;
    http_req.str = ev->buff.ptr;

    if( wbt_strpos( &http_req, &http_header_end ) < 0 ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_http_parse_request_header( wbt_event_t *ev ) {
    wbt_http_t *http = ev->data.ptr;

    http->uri.start = 0;
    http->uri.len = 0;
    http->version.start = 0;
    http->version.len = 0;
    
    wbt_str_t http_req;

    http_req.len = ev->buff.len;
    http_req.str = ev->buff.ptr;
    
    int offset = 0, state = 0, error = 0;
    char ch;
    wbt_mem_t tmp;
    wbt_http_header_t * header, * tail = NULL;
    while( error == 0 && offset < http_req.len ) {
        ch = http_req.str[offset];

        switch( state ) {
            case 0:
                if( ch == ' ' ) {
                    wbt_str_t method;
                    method.str = http_req.str;
                    method.len = offset;
                    
                    http->method = METHOD_UNKNOWN;
                    int i;
                    for(i = METHOD_UNKNOWN + 1 ; i < METHOD_LENGTH ; i++ ) {
                        if( wbt_strcmp(&method,
                                &REQUEST_METHOD[i],
                                REQUEST_METHOD[i].len ) == 0 ) {
                            http->method = i;
                            break;
                        }
                    }
                    
                    state = 1;
                }
                break;
            case 1:
                if( http->uri.start == 0 ) {
                    http->uri.start = offset;
                }
                if( ch == ' ' ) {
                    http->uri.len = offset - http->uri.start;
                    
                    state = 2;
                }
                break;
            case 2:
                if( http->version.start == 0 ) {
                    http->version.start = offset;
                }
                if( ch == '\r' ) {
                    http->version.len = offset - http->version.start;
                    
                    state = 3;
                }
                break;
            case 3:
                if( ch == '\n' ) {
                    /* 生成一个新的 wbt_http_header_t 并加入到链表中 */
                    /* 待优化，解析每个 http 都需要多次 malloc 影响性能 */
                    wbt_malloc( &tmp, sizeof( wbt_http_header_t ) );
                    wbt_memset( &tmp, 0 );
                    header = (wbt_http_header_t *)tmp.ptr;

                    if( http->headers == NULL ) {
                        http->headers = tail = header;
                    } else {
                        if( tail != NULL ) {
                            tail->next = header;
                            tail = header;
                        } else {
                            /* 不应该出现这个错误 */
                            wbt_log_add("wbt_http_parse_request_header error\n");
                            wbt_free(&tmp);

                            return WBT_ERROR;
                        }
                    }

                    state = 4;
                } else {
                    error = 1;
                }
                break;
            case 4:
                /* 开始解析 request line */
                if( tail->name.o.start == 0 ) {
                    tail->name.o.start = offset;
                }
                if( ch == ':' ) {
                    /* name 已经读完 */
                    tail->name.o.len = offset - tail->name.o.start;
                    
                    /* 检测该name */
                    int i;
                    wbt_str_t header_name;
                    wbt_offset_to_str(tail->name.o, header_name, ev->buff.ptr);
                    tail->key = HEADER_UNKNOWN;
                    for( i = 1 ; i < HEADER_LENGTH ; i ++ ) {
                        if( wbt_strcmp( &header_name, &HTTP_HEADERS[i], HTTP_HEADERS[i].len ) == 0 ) {
                            tail->key = i;
                            break;
                        }
                    }
                    
                    state = 5;
                } if( ch == '\r' ) {
                    if( tail->name.o.start == offset ) {
                        /* header 即将结束 */
                        state = 7;
                    } else {
                        error = 1;
                    }
                }
                break;
            case 5:
                if( ch == ' ' ) {
                    state = 6;
                } else {
                    error = 1;
                }
                break;
            case 6:
                /* 开始读取 value */
                if( tail->value.o.start == 0 ) {
                    tail->value.o.start = offset;
                }
                if( ch == '\r' ) {
                    /* value 已经读完 */
                    tail->value.o.len = offset - tail->value.o.start;
                    
                    state = 3;
                }
                break;
            case 7:
                if( ch == '\n' ) {
                    /* 移除尾端多余的一个node */
                    header = http->headers;

                    if( header != NULL ) {
                        if( header != tail ) { /* Bugfix: 当请求不包含任何header时不应当再发生段错误 */
                            while( header->next != tail ) header = header->next;
                            header->next = NULL;
                        } else {
                            http->headers = NULL;
                        }
                        
                        wbt_mem_t tmp;
                        tmp.ptr = tail;
                        tmp.len = 1;
                        wbt_free( &tmp );
                    }

                    state = 8;
                    error = 2;
                } else {
                    error = 1;
                }
                break;
        }

        offset ++;
    }
    
    http->body.start = offset;
    http->body.len = 0;
    
    if( error == 1 || state < 7 ) {
        /* Bad Request */
        http->status = STATUS_400;
        return WBT_ERROR;
    }

    wbt_log_debug(" METHOD: [%.*s]",
            REQUEST_METHOD[http->method].len,
            REQUEST_METHOD[http->method].str );
    wbt_log_debug("    URI: [%.*s]",
            http->uri.len,
            (char *)ev->buff.ptr + http->uri.start );
    wbt_log_debug("VERSION: [%.*s]",
            http->version.len,
            (char *)ev->buff.ptr + http->version.start );

    /* 检查 URI 长度 */
    if( http->uri.len >= 512 ) {
        /* 414 Request-URI Too Large */
        http->status = STATUS_414;
        return WBT_ERROR;
    }

    /* 检查 HTTP 版本信息 */
    wbt_str_t http_version;
    wbt_offset_to_str(http->version, http_version, ev->buff.ptr);
    if( wbt_strcmp( &http_version, &http_ver_1_0, http_ver_1_0.len ) != 0 &&
        wbt_strcmp( &http_version, &http_ver_1_1, http_ver_1_1.len ) != 0 ) {
        /* HTTP Version not supported */
        http->status = STATUS_505;
        return WBT_ERROR;
    }
    
    /* 解析 request header */
    wbt_str_t http_header_value;
    header = http->headers;
    while( header != NULL ) {
        switch( header->key ) {
            case HEADER_UNKNOWN:
                /* 忽略未知的 header 
                wbt_log_debug(" HEADER: [%.*s: %.*s] UNKNOWN",
                    header->name.len, header->name.str,
                    header->value.len, header->value.str); */
                break;
            case HEADER_CONNECTION:
                wbt_offset_to_str(header->value.o, http_header_value, ev->buff.ptr);
                if( wbt_stricmp( &http_header_value, &header_connection_keep_alive, header_connection_keep_alive.len ) == 0 ) {
                    /* 声明为 keep-alive 连接 */
                    http->bit_flag |= WBT_HTTP_KEEP_ALIVE;
                } else {
                    http->bit_flag &= ~WBT_HTTP_KEEP_ALIVE;
                }
                break;
            case HEADER_IF_MODIFIED_SINCE:
                /* 此时还没有读取文件，因此先记录此项 */
                http->bit_flag |= WBT_HTTP_IF_MODIFIED_SINCE;
                break;
        }

        header = header->next;
    }

    return WBT_OK;
}

wbt_status wbt_http_check_body_end( wbt_event_t *ev ) {
    wbt_http_t *http = ev->data.ptr;
    
    if( http->method == METHOD_GET ) {
        /* GET 请求没有body数据，所以不论是否有body都返回 WBT_OK */
        return WBT_OK;
    } else if( http->method == METHOD_POST ) {
        /* POST 请求需要根据 Content-Length 检查body长度 */
        int content_len = 0;
        
        wbt_http_header_t * header;
        wbt_str_t http_header_value;
        header = http->headers;
        while( header != NULL ) {
            if( header->key == HEADER_CONTENT_LENGTH ) {
                wbt_offset_to_str(header->value.o, http_header_value, ev->buff.ptr);
                content_len = wbt_atoi(&http_header_value);
                break;
            }

            header = header->next;
        }

        http->body.len = ev->buff.len - http->body.start;

        if( http->body.len >= content_len ) {
            return WBT_OK;
        } else {
            return WBT_ERROR;
        }
    }

    return WBT_OK;
}

wbt_status wbt_http_set_header( wbt_http_t* http, wbt_http_line_t key, wbt_str_t * value ) {
    wbt_http_header_t * header, * tail;

    /* 创建新的节点 */
    wbt_mem_t tmp;
    wbt_malloc( &tmp, sizeof( wbt_http_header_t ) );
    wbt_memset( &tmp, 0 );
    header = (wbt_http_header_t *)tmp.ptr;
    header->key = key;
    header->value.s = *(value);

    if( http->resp_headers == NULL ) {
        http->resp_headers = header;
    } else {
        tail = http->resp_headers;
        while( tail->next != NULL ) tail = tail->next;
        tail->next = header;
    }
    
    return WBT_OK;
}

wbt_status wbt_http_generate_response_header( wbt_http_t * http ) {
    /* 释放掉上次生成的数据，以免多次调用本方法出现内存泄漏 */
    wbt_free( &http->response );
    
    /* 计算并申请所需要的内存 */
    int mem_len = 13;    // "HTTP/1.1 " CRLF CRLF
    mem_len += STATUS_CODE[http->status].len;
    wbt_http_header_t * header = http->resp_headers;
    while( header != NULL ) {
        mem_len += HTTP_HEADERS[header->key].len;
        mem_len += header->value.s.len;
        mem_len += 4;   // ": " CRLF
            
        header = header->next;
    }
    mem_len += http->resp_body.len;
    
    if( wbt_malloc( &http->response, mem_len ) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    /* 生成响应消息头 */
    wbt_str_t dest;
    wbt_str_t crlf = wbt_string(CRLF);
    wbt_str_t space = wbt_string(" ");
    wbt_str_t colonspace = wbt_string(": ");
    dest.str = (u_char *)http->response.ptr;
    dest.len = 0;
    /* "HTTP/1.1 200 OK\r\n" */
    wbt_strcat( &dest, &http_ver_1_1, mem_len );
    wbt_strcat( &dest, &space, mem_len );
    wbt_strcat( &dest, &STATUS_CODE[http->status], mem_len );
    wbt_strcat( &dest, &crlf, mem_len );
    /* headers */
    header = http->resp_headers;
    while( header != NULL ) {
        wbt_strcat( &dest, &HTTP_HEADERS[header->key], mem_len );
        wbt_strcat( &dest, &colonspace, mem_len );
        wbt_strcat( &dest, &header->value.s, mem_len );
        wbt_strcat( &dest, &crlf, mem_len );
            
        header = header->next;
    }
    wbt_strcat( &dest, &crlf, mem_len );
    wbt_strcat( &dest, &http->resp_body, mem_len );
    
    wbt_log_debug("malloc: %d, use: %d", mem_len, dest.len );
    
    return WBT_OK;
}

wbt_status wbt_http_on_recv( wbt_event_t *ev ) {
    wbt_http_t * http = ev->data.ptr;
    wbt_log_debug("%.*s",ev->buff.len, (char *)ev->buff.ptr);

    if( http->state == STATE_CONNECTION_ESTABLISHED ||
            http->state == STATE_SEND_COMPLETED ) {
        http->state = STATE_RECEIVING_HEADER;
    }
    
    /* 检查是否已解析过 */
    if( http->status > STATUS_UNKNOWN ) {
        return WBT_ERROR;
    }
    
    if( ev->buff.len > 40960 ) {
         /* 413 Request Entity Too Large */
         http->status = STATUS_413;
         http->state = STATE_READY_TO_SEND;
         return WBT_OK;
    }
    
    if( http->state == STATE_RECEIVING_HEADER ) {
        /* 检查是否读完 http 消息头 */
        if( wbt_http_check_header_end( ev ) != WBT_OK ) {
            /* 尚未读完，继续等待数据，直至超时 */
            return WBT_OK;
        }
        
        http->state = STATE_PARSING_HEADER;
    }
    
    if( http->state == STATE_PARSING_HEADER ) {
        /* 解析 http 消息头 */
        if( wbt_http_parse_request_header( ev ) != WBT_OK ) {
            /* 消息头格式不正确，具体状态码由所调用的函数设置 */
            http->state = STATE_READY_TO_SEND;
            return WBT_OK;
        }
        
        http->state = STATE_RECEIVING_BODY;
    }
    
    // TODO 需要添加 wbt_http_parse_request_body
    if( http->state == STATE_RECEIVING_BODY ) {
        /* 检查是否读完 http 消息体 */
        if( wbt_http_check_body_end( ev ) != WBT_OK ) {
            /* 尚未读完，继续等待数据，直至超时 */
            return WBT_OK;
        }

        http->state = STATE_PARSING_REQUEST;
    }

    if( http->state == STATE_PARSING_REQUEST ) {
        /* 请求消息已经读完 */
        //wbt_log_add("%.*s\n", http->uri.len, http->uri.str);

        /* 打开所请求的文件 */
        // 判断 URI 是否以 / 开头
        // 判断 URI 中是否包含 ..
        wbt_str_t tmp_str = wbt_string(".."), http_uri;
        wbt_offset_to_str(http->uri, http_uri, ev->buff.ptr);
        if( *(http_uri.str) != '/'  || wbt_strpos( &http_uri, &tmp_str ) != -1 ) {
            // 合法的 HTTP 请求中 URI 应当以 / 开头
            // 也可以以 http:// 或 https:// 开头，但暂时不支持
            http->status = STATUS_400;
            http->state = STATE_READY_TO_SEND;
            return WBT_OK;
        }

        // 根据 URI 是否以 / 结束选择合适的补全方式
        wbt_str_t full_path;
        if( *(http_uri.str + http_uri.len - 1) == '/' ) {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s%.*s",
                wbt_conf.root.len, wbt_conf.root.str,
                http_uri.len, http_uri.str,
                wbt_conf.index.len, wbt_conf.index.str);
        } else {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
                wbt_conf.root.len, wbt_conf.root.str,
                http_uri.len, http_uri.str);
        }

        wbt_file_t *tmp = wbt_file_open( &full_path );
        if( tmp->fd < 0 ) {
            if( tmp->fd  == -1 ) {
                /* 文件不存在 */
                http->status = STATUS_404;
            } else if( tmp->fd  == -2 ) {
                /* 试图访问目录 */
                http->status = STATUS_403;
            } else if( tmp->fd  == -3 ) {
                /* 路径过长 */
                http->status = STATUS_414;
            }
            http->state = STATE_READY_TO_SEND;
            return WBT_OK;
        }

        http->state = STATE_READY_TO_SEND;
        http->status = STATUS_200;
        http->file.fd = tmp->fd;
        http->file.size = tmp->size;
        http->file.last_modified = tmp->last_modified;
        if( !wbt_conf.sendfile ) {
            /* TODO 在 open file 的时候读取文件内容并缓存 */
            wbt_file_read( &http->file );
        }
        http->file.offset = 0;
    }
    
    return WBT_OK;
}

wbt_status wbt_http_generater( wbt_event_t *ev ) {
    wbt_http_t * http = ev->data.ptr;
    
    wbt_str_t *t = (wbt_str_t *)&ev->buff;

    switch( http->state ) {
        case STATE_READY_TO_SEND:
            /* 需要返回响应 */
            if( wbt_http_process(ev) != WBT_OK ) {
                wbt_conn_close(ev);
            } else {
                http->state = STATE_SENDING;

                /* 等待socket可写 */
                ev->on_send = wbt_on_send;
                ev->events = EPOLLOUT | EPOLLET;
                ev->timeout = cur_mtime + wbt_conf.event_timeout;

                if(wbt_event_mod(ev) != WBT_OK) {
                    return WBT_ERROR;
                }
            }

            break;
        case STATE_RECEIVING_HEADER:
        case STATE_RECEIVING_BODY:
            /* 需要继续等待数据 */
            break;
        case STATE_BLOCKING:
            /* 供自定义模块使用以阻塞请求不立即返回 */
            break;
        default:
            /* 严重的错误，直接断开连接 */
            wbt_conn_close(ev);
    }
    
    return WBT_OK;
}

wbt_status wbt_http_process(wbt_event_t *ev) {   
    wbt_http_t * http = ev->data.ptr;
    /* 生成响应消息头 */
    wbt_http_set_header( http, HEADER_SERVER, &header_server );
    wbt_http_set_header( http, HEADER_DATE, &wbt_time_str_http );
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE && !wbt_wating_to_exit ) {
        wbt_http_set_header( http, HEADER_CONNECTION, &header_connection_keep_alive );
    } else {
        wbt_http_set_header( http, HEADER_CONNECTION, &header_connection_close );
    }
    wbt_str_t send_buf;
    if( http->status == STATUS_200 ) {
        if( http->file.fd > 0 && ( http->bit_flag & WBT_HTTP_IF_MODIFIED_SINCE ) ) {
            wbt_str_t * last_modified = wbt_time_to_str( http->file.last_modified );
            wbt_str_t header_value;
            wbt_http_header_t * header = http->headers;
            while( header != NULL ) {
                wbt_offset_to_str(header->value.o, header_value, ev->buff.ptr);
                if( header->key == HEADER_IF_MODIFIED_SINCE &&
                    wbt_strcmp2( last_modified, &header_value ) == 0 ) {
                    /* 304 Not Modified */
                    http->status = STATUS_304;
                }
                header = header->next;
            }
        }

        if( http->status == STATUS_200 ) {
            send_buf = wbt_sprintf(&wbt_send_buf, "%d", http->file.size);
        } else {
            /* 目前，这里可能是 304 */
            send_buf = wbt_sprintf(&wbt_send_buf, "%d", 0);
        }
        
        if( http->file.fd > 0 ) {
            wbt_http_set_header( http, HEADER_EXPIRES, &wbt_time_str_expire );
            wbt_http_set_header( http, HEADER_CACHE_CONTROL, &header_cache_control );
            wbt_http_set_header( http, HEADER_LAST_MODIFIED, wbt_time_to_str( http->file.last_modified ) );
        }
    } else {
        send_buf = wbt_sprintf(&wbt_send_buf, "%d", wbt_http_error_page[http->status].len);
        
        wbt_http_set_header( http, HEADER_CONTENT_TYPE, &header_content_type_text_html );
        
        http->resp_body = wbt_http_error_page[http->status];
    }
    wbt_http_set_header( http, HEADER_CONTENT_LENGTH, &send_buf );
    
    if( wbt_http_generate_response_header( http ) != WBT_OK ) {
        /* 内存不足，生成响应消息头失败 */
        return WBT_ERROR;
        
    }
    wbt_log_debug("%.*s", http->response.len, (char *)http->response.ptr);
    
    return WBT_OK;
}
