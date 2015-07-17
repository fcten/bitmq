#include "wbt_module_helloworld.h"
#include "../common/wbt_module.h"

wbt_module_t wbt_module_helloworld = {
    wbt_string("helloworld"),
    NULL, // init
    NULL, // exit
    NULL, // on_conn
    NULL, // on_recv
    NULL, // on_send
    wbt_module_helloworld_filter //on_filter
};

wbt_status wbt_module_helloworld_filter(wbt_http_t *http) {
    // 只过滤 404 响应
    if( http->status != STATUS_404 ) {
        return WBT_OK;
    }

    wbt_str_t resp = wbt_string("<html><body>hello world</body></html>");
    wbt_mem_t tmp;
    wbt_malloc(&tmp,resp.len);
    wbt_memcpy(&tmp, (wbt_mem_t *)&resp, resp.len);
    
    http->status = STATUS_200;
    http->file.ptr = tmp.ptr;
    http->file.size = tmp.len;
    
    return WBT_OK;
}