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
#include "wbt_http.h"

wbt_module_t wbt_module_http = {
    wbt_string("http"),
    wbt_http_init,
    NULL
};

wbt_mem_t wbt_file_path;
wbt_mem_t wbt_default_file; 
wbt_mem_t wbt_send_buf;

wbt_status wbt_http_init() {
    /* 初始化 sendbuf 与 file_path */
    wbt_malloc(&wbt_send_buf, 10240);
    wbt_malloc(&wbt_file_path, 512);
    wbt_malloc(&wbt_default_file, 256);
    
    wbt_mem_t * default_file = wbt_conf_get_v("default");
    wbt_memcpy(&wbt_default_file, default_file, default_file->len);
    if( default_file->len >= 255 ) {
        *((u_char *)wbt_default_file.ptr + 255) = '\0';
    } else {
        *((u_char *)wbt_default_file.ptr + default_file->len) = '\0';
    }
    
    return WBT_OK;
}

/* 释放 http 结构体中动态分配的内存，并重新初始化结构体 */
wbt_status wbt_http_destroy( wbt_http_t* http ) {
    wbt_http_header_t * header, * next;
    wbt_mem_t tmp;
    
    /* 关闭已打开的文件（不是真正的关闭，只是声明该文件已经在本次处理中使用完毕） */
    if( http->file.fd > 0 ) {
        // TODO 需要判断 URI 是否以 / 开头
        // TODO 需要和前面的代码整合到一起
        wbt_str_t full_path;
        if( *(http->uri.str + http->uri.len - 1) == '/' ) {
            full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
                http->uri.len, http->uri.str,
                wbt_default_file.len, wbt_default_file.ptr);
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
    
    /* 释放接收到的请求数据 */
    wbt_free( &http->buff );
    
    /* 初始化结构体
     * 由于 keep-alive 请求会重用这段内存，必须重新初始化
     */
    tmp.ptr = http;
    tmp.len = sizeof( wbt_http_t );
    wbt_memset( &tmp, 0 );

    return WBT_OK;
}

wbt_status wbt_http_check_header_end( wbt_http_t* http ) {
    wbt_str_t http_req;
    wbt_str_t http_header_end = wbt_string("\r\n\r\n");

    http_req.len = http->buff.len;
    http_req.str = http->buff.ptr;

    if( wbt_strpos( &http_req, &http_header_end ) < 0 ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_http_parse_request_header( wbt_http_t* http ) {
    wbt_str_t http_req;

    http_req.len = http->buff.len;
    http_req.str = http->buff.ptr;
    
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
                            wbt_log_add("wbt_http_parse_request_header error");
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
        /* TODO POST 请求需要根据 Content-Length 检查body长度 */
        return WBT_OK;
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

wbt_status wbt_http_parse( wbt_http_t * http ) {
    //printf("------\n%.*s\n------\n", ev->data.buff.len, ev->data.buff.ptr);
    /* 检查是否已解析过 */
    if( http->status > 0 ) {
        return WBT_ERROR;
    }

    /* 检查是否读完 http 消息头 */
    if( wbt_http_check_header_end( http ) != WBT_OK ) {
        /* 尚未读完，继续等待数据，直至超时 */
        return WBT_OK;
    }
    
    /* 解析 http 消息头 */
    if( wbt_http_parse_request_header( http ) != WBT_OK ) {
        /* 消息头格式不正确，具体状态码由所调用的函数设置 */
        return WBT_ERROR;
    }

    /* 检查是否读完 http 消息体 */
    if( wbt_http_check_body_end( http ) != WBT_OK ) {
        /* 尚未读完，继续等待数据，直至超时 */
        return WBT_OK;
    }

    /* 请求消息已经读完 */
    //wbt_log_add("%.*s\n", http->uri.len, http->uri.str);

    /* 打开所请求的文件 */
    // TODO 需要判断 URI 是否以 / 开头
    wbt_str_t full_path;
    if( *(http->uri.str + http->uri.len - 1) == '/' ) {
        full_path = wbt_sprintf(&wbt_file_path, "%.*s%.*s",
            http->uri.len, http->uri.str,
            wbt_default_file.len, wbt_default_file.ptr);
    } else {
        full_path = wbt_sprintf(&wbt_file_path, "%.*s",
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
        return WBT_ERROR;
    }
    http->status = STATUS_200;
    http->file.fd = tmp->fd;
    http->file.size = tmp->size;
    http->file.last_modified = tmp->last_modified;
    http->file.offset = 0;
    
    return WBT_OK;
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
        wbt_str_t * last_modified = wbt_time_to_str( http->file.last_modified );
        if( http->bit_flag & WBT_HTTP_IF_MODIFIED_SINCE ) {
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
        
        wbt_http_set_header( http, HEADER_EXPIRES, &wbt_time_str_expire );
        wbt_http_set_header( http, HEADER_CACHE_CONTROL, &header_cache_control );
        
        wbt_http_set_header( http, HEADER_LAST_MODIFIED, wbt_time_to_str( http->file.last_modified ) );
    } else {
        send_buf = wbt_sprintf(&wbt_send_buf, "%d", wbt_http_error_page[http->status].len);
        
        wbt_http_set_header( http, HEADER_CONTENT_TYPE, &header_content_type_text_html );
        
        http->resp_body = wbt_http_error_page[http->status];
    }
    wbt_http_set_header( http, HEADER_CONTENT_LENGTH, &send_buf );
    
    wbt_http_generate_response_header( http );
    wbt_log_debug("%.*s", http->response.len, http->response.ptr);
    
    return WBT_OK;
}