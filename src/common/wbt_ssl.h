/* 
 * File:   wbt_ssl.h
 * Author: fcten
 *
 * Created on 2015年12月8日, 下午8:57
 */

#ifndef WBT_SSL_H
#define	WBT_SSL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

wbt_status wbt_ssl_init();
wbt_status wbt_ssl_exit();
wbt_status wbt_ssl_on_conn( wbt_event_t *ev );
wbt_status wbt_ssl_on_close( wbt_event_t *ev );

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_SSL_H */

