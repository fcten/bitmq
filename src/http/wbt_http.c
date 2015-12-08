 /* 
 * File:   wbt_http.c
 * Author: Fcten
 *
 * Created on 2014年10月24日, 下午3:30
 */


#include "../common/wbt_module.h"
#include "../common/wbt_memory.h"
#include "../common/wbt_log.h"
#include "../common/wbt_time.h"
#include "../common/wbt_config.h"
#include "../common/wbt_connection.h"
#include "wbt_http.h"

wbt_status wbt_http_init();
wbt_status wbt_http_check_header_end( wbt_event_t * );
wbt_status wbt_http_parse_request_header( wbt_event_t * );
wbt_status wbt_http_check_body_end( wbt_http_t* );
wbt_status wbt_http_set_header( wbt_http_t*, wbt_http_line_t, wbt_str_t* );
wbt_status wbt_http_generate_response_header( wbt_http_t* );


wbt_status wbt_http_process( wbt_http_t * );


wbt_status wbt_http_on_conn( wbt_event_t * );
wbt_status wbt_http_on_recv( wbt_event_t * );
wbt_status wbt_http_on_send( wbt_event_t * );
wbt_status wbt_http_on_close( wbt_event_t * );

wbt_status wbt_http_generater( wbt_event_t * );

wbt_module_t wbt_module_http1_parser = {
    wbt_string("http-parser"),
    wbt_http_init,
    NULL,
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
        return WBT_ERROR;
    }
    
    /* TODO 发送大文件时，使用 TCP_CORK 关闭 Nagle 算法保证网络利用率 */
    //int on = 1;
    //setsockopt( conn_sock, SOL_TCP, TCP_CORK, &on, sizeof ( on ) );

    /* 如果存在 response，发送 response */
    if( http->response.len - http->resp_offset > 0 ) {
        n = http->response.len - http->resp_offset; 
        nwrite = write(ev->fd, http->response.ptr + http->resp_offset, n);

        if (nwrite == -1 && errno != EAGAIN) {
            /* 这里数据发送失败了，但应当返回 WBT_OK。这个函数的返回值
             * 仅用于判断是否发生了必须重启工作进程的严重错误。
             * 如果模块需要处理数据发送失败的错误，必须根据 state 在 on_close 回调中处理。
             */
            wbt_conn_close(ev);
            
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
            nwrite = write(ev->fd, http->file.ptr + http->file.offset, n);
            http->file.offset += nwrite;
        } else if( http->file.fd > 0  ) {
            // 需要发送的数据不在内存中，但是指定了需要发送的文件
            nwrite = sendfile( ev->fd, http->file.fd, &http->file.offset, n );
        } else {
            // 不应该出现这种情况
            n = nwrite = 0;
        }

        if (nwrite == -1 && errno != EAGAIN) { 
            /* 连接被意外关闭，同上 */
            wbt_conn_close(ev);
            
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
    
    /* 如果是 keep-alive 连接，继续等待数据到来 */
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE ) {
        /* 为下一个连接初始化相关结构 */
        wbt_http_on_close( ev );
        if( wbt_malloc(&ev->data, sizeof(wbt_http_t)) != WBT_OK ) {
            return WBT_ERROR;
        }
        wbt_memset(&ev->data, 0);

        ev->trigger = wbt_on_recv;
        ev->events = EPOLLIN | EPOLLET;
        ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

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
    
    /* 关闭已打开的文件（不是真正的关闭，只是声明该文件已经在本次处理中使用完毕） */
    if( http->file.fd > 0 ) {
        // TODO 需要判断 URI 是否以 / 开头
        // TODO 需要和前面的代码整合到一起
        wbt_str_t full_path;
        if( *(http->uri.str + http->uri.len - 1) == '/' ) {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
                http->uri.len, http->uri.str,
                wbt_conf.index.len, wbt_conf.index.str);
        } else {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s",
                http->uri.len, http->uri.str);
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
    
    /* 释放 http 结构体本身 */
    wbt_free(&ev->data);

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
    
    /* 如果发生了重写，需要释放掉上一次生成的 header 链 */
    if( http->headers != NULL ) {
        wbt_http_header_t * header = http->headers, * next;
        wbt_mem_t tmp;
        
        while( header != NULL ) {
            next = header->next;

            tmp.ptr = header;
            tmp.len = 1;
            wbt_free( &tmp );

            header = next;
        }
        
        http->headers = NULL;
    }
    http->method.str = NULL;
    http->method.len = 0;
    http->uri.str = NULL;
    http->uri.len = 0;
    http->version.str = NULL;
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
                    http->method.str = http_req.str;
                    http->method.len = offset;
                    
                    state = 1;
                }
                break;
            case 1:
                if( http->uri.str == NULL ) {
                    http->uri.str = http_req.str + offset;
                }
                if( ch == ' ' ) {
                    http->uri.len = offset - ( http->uri.str - http_req.str );
                    
                    state = 2;
                }
                break;
            case 2:
                if( http->version.str == NULL ) {
                    http->version.str = http_req.str + offset;
                }
                if( ch == '\r' ) {
                    http->version.len = offset - ( http->version.str - http_req.str );
                    
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
                if( tail->name.str == NULL ) {
                    tail->name.str = http_req.str + offset;
                }
                if( ch == ':' ) {
                    /* name 已经读完 */
                    tail->name.len = offset - ( tail->name.str - http_req.str );
                    
                    /* 检测该name */
                    int i;
                    tail->key = HEADER_UNKNOWN;
                    for( i = 1 ; i < HEADER_LENGTH ; i ++ ) {
                        if( wbt_strcmp( &tail->name, &HTTP_HEADERS[i], HTTP_HEADERS[i].len ) == 0 ) {
                            tail->key = i;
                            break;
                        }
                    }
                    
                    state = 5;
                } if( ch == '\r' ) {
                    if( tail->name.str == ( http_req.str + offset ) ) {
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
                if( tail->value.str == NULL ) {
                    tail->value.str = http_req.str + offset;
                }
                if( ch == '\r' ) {
                    /* value 已经读完 */
                    tail->value.len = offset - ( tail->value.str - http_req.str );
                    
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
                } else {
                    error = 1;
                }
                break;
            case 8:
                http->body.str = http_req.str + offset;
                http->body.len = http_req.len - offset;
                
                error = 2;
                break;
        }

        offset ++;
    }
    
    if( error == 1 || state < 7 ) {
        /* Bad Request */
        http->status = STATUS_400;
        return WBT_ERROR;
    }

    wbt_log_debug(" METHOD: [%.*s]", http->method.len, http->method.str );
    wbt_log_debug("    URI: [%.*s]", http->uri.len, http->uri.str );
    wbt_log_debug("VERSION: [%.*s]", http->version.len, http->version.str );
    /* 检查 URI 长度 */
    if( http->uri.len >= 512 ) {
        /* 414 Request-URI Too Large */
        http->status = STATUS_414;
        return WBT_ERROR;
    }

    /* 检查 HTTP 版本信息 */
    if( wbt_strcmp( &http->version, &http_ver_1_0, http_ver_1_0.len ) != 0 &&
        wbt_strcmp( &http->version, &http_ver_1_1, http_ver_1_1.len ) != 0 ) {
        /* HTTP Version not supported */
        http->status = STATUS_505;
        return WBT_ERROR;
    }
    
    /* 解析 request header */
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
                if( wbt_strcmp( &header->value, &header_connection_keep_alive, header_connection_keep_alive.len ) == 0 ) {
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

wbt_status wbt_http_check_body_end( wbt_http_t* http ) {
    if( wbt_strcmp(&http->method, &REQUEST_METHOD[METHOD_GET], REQUEST_METHOD[METHOD_GET].len ) == 0 ) {
        /* GET 请求没有body数据，所以不论是否有body都返回 WBT_OK */
        return WBT_OK;
    } else if( wbt_strcmp(&http->method, &REQUEST_METHOD[METHOD_POST], REQUEST_METHOD[METHOD_POST].len ) == 0 ) {
        /* POST 请求需要根据 Content-Length 检查body长度 */
        int content_len = 0;
        
        wbt_http_header_t * header;
        header = http->headers;
        while( header != NULL ) {
            if( header->key == HEADER_CONTENT_LENGTH ) {
                content_len = wbt_atoi(&header->value);
                break;
            }

            header = header->next;
        }
        
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
    header->value = *(value);

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
        mem_len += header->value.len;
        mem_len += 4;   // ": " CRLF
            
        header = header->next;
    }
    mem_len += http->resp_body.len;
    wbt_malloc( &http->response, mem_len );
    
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
        wbt_strcat( &dest, &header->value, mem_len );
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
        if( wbt_http_check_body_end( http ) != WBT_OK ) {
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
        wbt_str_t tmp_str = wbt_string("..");
        if( *(http->uri.str) != '/'  || wbt_strpos( &http->uri, &tmp_str ) != -1 ) {
            // 合法的 HTTP 请求中 URI 应当以 / 开头
            // 也可以以 http:// 或 https:// 开头，但暂时不支持
            http->status = STATUS_400;
            http->state = STATE_READY_TO_SEND;
            return WBT_OK;
        }

        // 根据 URI 是否以 / 结束选择合适的补全方式
        wbt_str_t full_path;
        if( *(http->uri.str + http->uri.len - 1) == '/' ) {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s%.*s",
                wbt_conf.root.len, wbt_conf.root.str,
                http->uri.len, http->uri.str,
                wbt_conf.index.len, wbt_conf.index.str);
        } else {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
                wbt_conf.root.len, wbt_conf.root.str,
                http->uri.len, http->uri.str);
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
            return WBT_OK;
        }

        http->state = STATE_READY_TO_SEND;
        http->status = STATUS_200;
        http->file.fd = tmp->fd;
        http->file.size = tmp->size;
        http->file.last_modified = tmp->last_modified;
        http->file.offset = 0;
    }
    
    return WBT_OK;
}

wbt_status wbt_http_generater( wbt_event_t *ev ) {
    wbt_http_t * http = ev->data.ptr;

    switch( http->state ) {
        case STATE_READY_TO_SEND:
            /* 需要返回响应 */
            wbt_http_process(http);

            http->state = STATE_SENDING;

            /* 等待socket可写 */
            ev->trigger = wbt_on_send;
            ev->events = EPOLLOUT | EPOLLET;
            ev->time_out = cur_mtime + WBT_CONN_TIMEOUT;

            if(wbt_event_mod(ev) != WBT_OK) {
                return WBT_ERROR;
            }

            break;
        case STATE_RECEIVING_HEADER:
        case STATE_RECEIVING_BODY:
            /* 需要继续等待数据 */
            break;
        case STATE_PARSING_REQUEST:
            /* 供自定义模块使用以阻塞请求不立即返回 */
            break;
        default:
            /* 严重的错误，直接断开连接 */
            wbt_conn_close(ev);
    }
}

wbt_status wbt_http_process(wbt_http_t *http) {    
    /* 生成响应消息头 */
    wbt_http_set_header( http, HEADER_SERVER, &header_server );
    wbt_http_set_header( http, HEADER_DATE, &wbt_time_str_http );
    if( http->bit_flag & WBT_HTTP_KEEP_ALIVE ) {
        wbt_http_set_header( http, HEADER_CONNECTION, &header_connection_keep_alive );
    } else {
        wbt_http_set_header( http, HEADER_CONNECTION, &header_connection_close );
    }
    wbt_str_t send_buf;
    if( http->status == STATUS_200 ) {
        if( http->file.fd > 0 && ( http->bit_flag & WBT_HTTP_IF_MODIFIED_SINCE ) ) {
            wbt_str_t * last_modified = wbt_time_to_str( http->file.last_modified );
            wbt_http_header_t * header = http->headers;
            while( header != NULL ) {
                if( header->key == HEADER_IF_MODIFIED_SINCE &&
                    wbt_strcmp2( last_modified, &header->value ) == 0 ) {
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
    
    wbt_http_generate_response_header( http );
    wbt_log_debug("%.*s", http->response.len, (char *)http->response.ptr);
    
    return WBT_OK;
}
