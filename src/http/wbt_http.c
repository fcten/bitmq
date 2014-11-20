 /* 
 * File:   wbt_http.c
 * Author: Fcten
 *
 * Created on 2014年10月24日, 下午3:30
 */


#include "../common/wbt_module.h"
#include "../common/wbt_memory.h"
#include "../common/wbt_log.h"
#include "wbt_http.h"

wbt_module_t wbt_module_http = {
    wbt_string("http"),
    NULL
};

/* 释放 http 结构体中动态分配的内存，并重新初始化结构体 */
wbt_status wbt_http_destroy( wbt_http_t* http ) {
    wbt_http_header_t * header, * next;
    wbt_mem_t tmp;

    /* 释放动态分配的内存 */
    header = http->headers;
    while( header != NULL ) {
        next = header->next;
        
        tmp.ptr = header;
        tmp.len = 1;
        wbt_free( &tmp );

        header = next;
    }

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
                    tail->key = HEADER_LENGTH;
                    for( i = 0 ; i < HEADER_LENGTH ; i ++ ) {
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
        /* 400 Bad Request */
        return WBT_ERROR;
    }

    wbt_log_debug(" METHOD: [%.*s]", http->method.len, http->method.str );
    wbt_log_debug("    URI: [%.*s]", http->uri.len, http->uri.str );
    wbt_log_debug("VERSION: [%.*s]", http->version.len, http->version.str );

    /* 检查 HTTP 版本信息 */
    wbt_str_t http_ver = wbt_string("HTTP/1.1");
    if( wbt_strcmp( &http->version, &http_ver, http_ver.len ) != 0 ) {
        /* 400 Bad Request */
        return WBT_ERROR;
    }
    
    /* 解析 request header */
    header = http->headers;
    while( header != NULL ) {
        if( header->key == HEADER_LENGTH ) {
            /* 忽略未知的 header 
            wbt_log_debug(" HEADER: [%.*s: %.*s] UNKNOWN",
                header->name.len, header->name.str,
                header->value.len, header->value.str); */
        } else {
            /* 处理已知的 header 
            wbt_log_debug(" HEADER: [%.*s: %.*s]",
                HTTP_HEADERS[header->key].len, HTTP_HEADERS[header->key].str,
                header->value.len, header->value.str); */
        }

        header = header->next;
    }

    return WBT_OK;
}

wbt_status wbt_http_check_body_end( wbt_http_t* http ) {
    /* GET 请求没有body数据，所以不论是否有body都返回 WBT_OK */
    if( wbt_strcmp(&http->method, &REQUEST_METHOD[METHOD_GET], REQUEST_METHOD[METHOD_GET].len ) == 0 ) {
        return WBT_OK;
    }
    /* POST 请求需要根据 Content-Length 检查body长度 */
    return WBT_OK;
}