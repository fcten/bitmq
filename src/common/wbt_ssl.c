 /* 
 * File:   wbt_http.c
 * Author: Fcten
 *
 * Created on 2014年10月24日, 下午3:30
 */

#include "wbt_module.h"
#include "wbt_log.h"
#include "wbt_connection.h"
#include "wbt_config.h"
#include "wbt_ssl.h"

wbt_module_t wbt_module_ssl = {
    wbt_string("ssl"),
    wbt_ssl_init,
    wbt_ssl_exit,
    wbt_ssl_on_conn,
    NULL,
    NULL,
    wbt_ssl_on_close
};

SSL_CTX* ctx;

int wbt_ssl_ecdh_curve();

wbt_status wbt_ssl_init() {
    if( !wbt_conf.secure ) {
        return WBT_OK;
    }
    
    SSL_load_error_strings();     /*为打印调试信息作准备*/
    OpenSSL_add_ssl_algorithms(); /*初始化*/
    
    ctx = SSL_CTX_new(TLSv1_2_server_method());
    if (!ctx) {
        wbt_log_add("SSL_CTX_new failed\n");
        return WBT_ERROR;
    }
    
    // SSL/TLS有几个公认的bug,这样设置会使出错的可能更小
    SSL_CTX_set_options(ctx,SSL_OP_ALL);
    
    // 设置为要求强制校验对方（客户端）证书SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT
    SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,NULL); /*验证与否SSL_VERIFY_PEER*/ 
    // SSL_CTX_load_verify_locations(ctx,CACERT,NULL);  /*若验证,则放置CA证书*/
    
    if (SSL_CTX_use_certificate_file(ctx, wbt_stdstr(&wbt_conf.secure_crt), SSL_FILETYPE_PEM) <= 0) {
        wbt_log_add("SSL_CTX_use_certificate_file failed\n");
        return WBT_ERROR;
    }
 
    if (SSL_CTX_use_PrivateKey_file(ctx, wbt_stdstr(&wbt_conf.secure_key), SSL_FILETYPE_PEM) <= 0) {
        wbt_log_add("SSL_CTX_use_PrivateKey_file failed\n");
        return WBT_ERROR;
    }
   
    if (!SSL_CTX_check_private_key(ctx)) {
        wbt_log_add("SSL_CTX_check_private_key failed\n");
        return WBT_ERROR;
    }
 
    if ( SSL_CTX_set_cipher_list(ctx,"ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-RC4-SHA:ECDHE-RSA-RC4-SHA:ECDH-ECDSA-RC4-SHA:ECDH-RSA-RC4-SHA:ECDHE-RSA-AES256-SHA:RC4-SHA:HIGH:!aNULL:!eNULL:!LOW:!3DES:!MD5:!EXP:!CBC:!EDH:!kEDH:!PSK:!SRP:!kECDH") <= 0 ) {
        wbt_log_add("SSL_CTX_set_cipher_list failed\n");
        return WBT_ERROR;
    }
    
    if( wbt_ssl_ecdh_curve() != WBT_OK ) {
        return WBT_ERROR;
    }

    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    /* TODO 根据情况决定是否 quiet shutdown。对已关闭的连接调用 SSL_shutdown 会导致 broken pipe */
    SSL_CTX_set_quiet_shutdown(ctx, 1);
    
    return WBT_OK;
}

wbt_status wbt_ssl_exit() {
    if( !wbt_conf.secure ) {
        return WBT_OK;
    }

    SSL_CTX_free (ctx);
    
    return WBT_OK;
}

wbt_status wbt_ssl_handshake(wbt_event_t *ev) {
    int n = SSL_do_handshake(ev->ssl);

    if( n == 1 ) {
        /*打印所有加密算法的信息(可选)*/ 
        printf ("SSL connection using %s\n", SSL_get_cipher (ev->ssl));

        ev->on_recv = wbt_on_recv;
        ev->events = EPOLLIN | EPOLLET;
        ev->timeout = cur_mtime + wbt_conf.event_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
        
        return WBT_OK;
    }
    
    int err = SSL_get_error(ev->ssl, n);

    wbt_log_debug("SSL_get_error: %d", err);
    wbt_log_debug("Error: %s", ERR_reason_error_string(ERR_get_error()));

    if( err == SSL_ERROR_WANT_READ ) {
        ev->on_recv = wbt_ssl_handshake;
        ev->events = EPOLLIN | EPOLLET;
        ev->timeout = cur_mtime + wbt_conf.event_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
        
        return WBT_OK;
    } else if( err == SSL_ERROR_WANT_WRITE ) {
        ev->on_send = wbt_ssl_handshake;
        ev->events = EPOLLOUT | EPOLLET;
        ev->timeout = cur_mtime + wbt_conf.event_timeout;

        if(wbt_event_mod(ev) != WBT_OK) {
            return WBT_ERROR;
        }
        
        return WBT_OK;
    } else if( err == SSL_ERROR_SYSCALL ) {
        return WBT_OK;
    }
    
    // 握手异常，可能客户端在握手期间关闭了连接
    wbt_conn_close(ev);

    return WBT_OK;
}

wbt_status wbt_ssl_on_conn( wbt_event_t * ev ) {
    if( !wbt_conf.secure ) {
        return WBT_OK;
    }

    ev->ssl = SSL_new (ctx);
    if(!ev->ssl) {
        wbt_log_add("SSL_new failed\n");
        return WBT_ERROR;
    }

    if( SSL_set_fd (ev->ssl, ev->fd) == 0 ) {
        wbt_log_add("SSL_set_fd failed\n");
        return WBT_ERROR;
    }
    
    SSL_set_accept_state(ev->ssl);
    
    if( wbt_ssl_handshake(ev) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    /*int err = SSL_accept (ev->ssl);
    if(err == 0) {
        wbt_log_debug("SSL_get_error: %d", SSL_get_error(ev->ssl, err));
        wbt_log_debug("Error: %s", ERR_reason_error_string(ERR_get_error()));
        
        wbt_conn_close(ev);
        return WBT_OK;
    }*/
   
  /*得到客户端的证书并打印些信息(可选) */ 
  /*client_cert = SSL_get_peer_certificate (ssl);
  if (client_cert != NULL) 
  {
    printf ("Client certificate:\n");
     
    str = X509_NAME_oneline (X509_get_subject_name (client_cert), 0, 0);
    CHK_NULL(str);
    printf ("\t subject: %s\n", str);
    OPENSSL_free (str);
     
    str = X509_NAME_oneline (X509_get_issuer_name  (client_cert), 0, 0);
    CHK_NULL(str);
    printf ("\t issuer: %s\n", str);
    OPENSSL_free (str);
     
    //如不再需要,需将证书释放
    X509_free (client_cert);
  } 
  else
    printf ("Client does not have certificate.\n");*/
    
    return WBT_OK;
}

wbt_status wbt_ssl_on_close( wbt_event_t * ev ) {
    if( !wbt_conf.secure ) {
        return WBT_OK;
    }

    SSL_shutdown(ev->ssl);
    SSL_free (ev->ssl);
    
    return WBT_OK;
}

int wbt_ssl_read(wbt_event_t *ev, void *buf, int num) {
    if( wbt_conf.secure ) {
        return SSL_read(ev->ssl, buf, num);
    } else {
        return read(ev->fd, buf, num);
    }
}

int wbt_ssl_write(wbt_event_t *ev, const void *buf, int num) {
    if( wbt_conf.secure ) {
        return SSL_write(ev->ssl, buf, num);
    } else {
        return write(ev->fd, buf, num);
    }
}

int wbt_ssl_ecdh_curve() {
#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#ifndef OPENSSL_NO_ECDH
    int      nid;
    EC_KEY  *ecdh;

    /*
     * Elliptic-Curve Diffie-Hellman parameters are either "named curves"
     * from RFC 4492 section 5.1.1, or explicitly described curves over
     * binary fields. OpenSSL only supports the "named curves", which provide
     * maximum interoperability.
     */

    nid = OBJ_sn2nid("prime256v1");
    if (nid == 0) {
        wbt_log_add("OBJ_sn2nid failed\n");
        return WBT_ERROR;
    }

    ecdh = EC_KEY_new_by_curve_name(nid);
    if (ecdh == NULL) {
        wbt_log_add("EC_KEY_new_by_curve_name failed\n");
        return WBT_ERROR;
    }

    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);

    SSL_CTX_set_tmp_ecdh(ctx, ecdh);

    EC_KEY_free(ecdh);
#endif
#endif

    return WBT_OK;
}

int wbt_ssl_get_error(wbt_event_t *ev, int ret) {
    return SSL_get_error(ev->ssl, ret);
}