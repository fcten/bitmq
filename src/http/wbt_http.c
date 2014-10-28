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

/* 释放 http 结构体中动态分配的内存 */
wbt_status wbt_http_destroy( wbt_http_t* http ) {
    wbt_mem_t * header, * next;

    header = http->headers;
    while( header != NULL ) {
        next = header->next;
        wbt_free( header );
        header = next;
    }

    wbt_free( &http->buff );

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
                if( ch == ' ' ) {
                    http->uri.str = http_req.str + http->method.len + 1;
                    http->uri.len = offset - http->method.len - 1;
                    
                    state = 2;
                }
                break;
            case 2:
                if( ch == '\r' ) {
                    http->version.str = http->uri.str + http->uri.len + 1;
                    http->version.len = offset - http->uri.len - http->method.len - 2;
                    
                    state = 3;
                }
                break;
            case 3:
                if( ch == '\n' ) {
                    state = 4;
                } else {
                    error = 1;
                }
                break;
            case 4:
                /* 开始解析 request line */
                break;
        }

        offset ++;
    }
    
    if( error == 1 || state != 4 ) {
        /* 400 Bad Request */
        return WBT_ERROR;
    }
    
    wbt_log_debug(" METHOD: [%.*s]", http->method.len, http->method.str );
    wbt_log_debug("    URI: [%.*s]", http->uri.len, http->uri.str );
    wbt_log_debug("VERSION: [%.*s]", http->version.len, http->version.str );
    
    //int i, flag = 0;
    /*for( i = 0 ;i < METHOD_LENGTH ; i ++ ) {
        if( wbt_strcmp( &http_req, &REQUEST_METHOD[i], REQUEST_METHOD[i].len ) == 0 ) {
            http->method = i;
            flag = 1;
            break;
        }
    }*/
    
    return WBT_OK;
}

wbt_status wbt_http_check_body_end( wbt_http_t* http ) {
    /* GET 请求没有body数据，所以不论是否有body都返回 WBT_OK */
    /* POST 请求需要根据 Content-Length 检查body长度 */
    return WBT_OK;
}