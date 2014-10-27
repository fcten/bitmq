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
    return WBT_OK;
}

wbt_status wbt_http_check_body_end( wbt_http_t* http ) {
    return WBT_OK;
}