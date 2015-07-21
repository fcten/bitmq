#include "wbt_module_helloworld.h"
#include "../common/wbt_rbtree.h"

wbt_module_t wbt_module_helloworld = {
    wbt_string("helloworld"),
    wbt_module_helloworld_init, // init
    NULL, // exit
    NULL, // on_conn
    wbt_module_helloworld_recv, // on_recv
    NULL, // on_send
    wbt_module_helloworld_close // on_close
};

wbt_rbtree_t wbt_online_fd_rbtree;
wbt_rbtree_t wbt_online_id_rbtree;

extern wbt_rbtree_node_t *wbt_rbtree_node_nil;
extern time_t cur_mtime;

wbt_status wbt_module_helloworld_init() {
    /* 初始化一个红黑树用以根据 fd 查找客户端 */
    wbt_rbtree_init(&wbt_online_fd_rbtree);
    /* 初始化一个红黑树用以根据 id 查找客户端 */
    wbt_rbtree_init(&wbt_online_id_rbtree);
    
    return WBT_OK;
}

wbt_status wbt_module_helloworld_conn(wbt_event_t *ev) {
    return WBT_OK;
}

wbt_status wbt_module_helloworld_close(wbt_event_t *ev) {
    /* 根据 fd 将客户端设置为离线 */
    return WBT_OK;
}

wbt_status wbt_module_helloworld_recv(wbt_event_t *ev) {
    wbt_http_t * http = &ev->data;

    // 只过滤 404 响应
    if( http->status != STATUS_404 ) {
        return WBT_OK;
    }
    
    wbt_str_t api = wbt_string("/api/");
    wbt_str_t login = wbt_string("/login/");
    wbt_str_t show = wbt_string("/show/");
    if( wbt_strcmp( &http->uri, &api, api.len ) == 0 ) {
        return WBT_OK;
    } else if( wbt_strcmp2( &http->uri, &login ) == 0 ) {
        return wbt_module_helloworld_login(ev);
    } else if( wbt_strcmp2( &http->uri, &show ) == 0 ) {
        return wbt_module_helloworld_show(ev);
    }
    
    return WBT_OK;
}

wbt_status wbt_module_helloworld_login(wbt_event_t *ev) {
    wbt_http_t * http = &ev->data;

    // 必须是 POST 请求
    if( wbt_strcmp(&http->method, &REQUEST_METHOD[METHOD_POST], REQUEST_METHOD[METHOD_POST].len ) != 0 ) {
        http->status = STATUS_405;
        return WBT_OK;
    }
    
    // 处理 body
    if( http->body.len != 32 ) {
        http->status = STATUS_413;
        return WBT_OK;
    }
    
    wbt_rbtree_node_t *node =  wbt_rbtree_get(&wbt_online_id_rbtree, &http->body);
    if( node == NULL ) {
        wbt_mem_t client;
        wbt_malloc(&client, sizeof(wbt_online_t));
        wbt_memset(&client, 0);

        node = wbt_rbtree_insert(&wbt_online_id_rbtree, &http->body);
        node->value = client;
    }
    
    wbt_online_t *p = (wbt_online_t *)node->value.ptr;
 
    if ( p->status == 1 && p->ev != ev ) {
        wbt_conn_close(p->ev);
    }

    if( p->status == 0 ) {
        p->ev = ev;
        wbt_mem_t tmp;
        tmp.ptr = p->id;
        tmp.len = 32;
        wbt_memcpy( &tmp, (wbt_mem_t *)&http->body, http->body.len );
        p->online = cur_mtime;
        p->status = 1;
    }
    
    http->status = STATUS_200;
    
    return WBT_OK;
}

void wbt_module_helloworld_show_recursion(wbt_rbtree_node_t *node, wbt_str_t *resp, int maxlen) {
    wbt_online_t *p = (wbt_online_t *)node->value.ptr;

    wbt_str_t online = wbt_string("online \n");
    wbt_str_t offline = wbt_string("offline\n");
    wbt_str_t id;
    id.str = p->id;
    id.len = 32;

    if(node != wbt_rbtree_node_nil) {
        wbt_module_helloworld_show_recursion(node->left, resp, maxlen);
        wbt_strcat(resp, &id, maxlen);
        if( p->status == 1 ) {
            wbt_strcat(resp, &online, maxlen);
        } else {
            wbt_strcat(resp, &offline, maxlen);
        }
        wbt_module_helloworld_show_recursion(node->right, resp, maxlen);
    }
}

wbt_status wbt_module_helloworld_show(wbt_event_t *ev) {
    wbt_http_t * http = &ev->data;
    
    // 必须是 GET 请求
    if( wbt_strcmp(&http->method, &REQUEST_METHOD[METHOD_GET], REQUEST_METHOD[METHOD_GET].len ) != 0 ) {
        http->status = STATUS_405;
        return WBT_OK;
    }

    wbt_mem_t tmp;
    wbt_malloc(&tmp, 10240);

    wbt_str_t resp;
    resp.len = 0;
    resp.str = tmp.ptr;

    wbt_module_helloworld_show_recursion(wbt_online_id_rbtree.root, &resp, tmp.len);
    wbt_realloc(&tmp, resp.len);
    
    http->status = STATUS_200;
    http->file.ptr = tmp.ptr;
    http->file.size = tmp.len;

    return WBT_OK;
}