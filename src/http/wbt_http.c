/* 
 * File:   wbt_http.c
 * Author: Fcten
 *
 * Created on 2014年10月24日, 下午3:30
 */


#include "../common/wbt_module.h"
#include "../common/wbt_memory.h"
#include "wbt_http.h"

wbt_module_t wbt_module_http = {
    wbt_string("http"),
    NULL
};

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