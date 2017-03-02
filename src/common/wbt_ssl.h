/* 
 * File:   wbt_ssl.h
 * Author: fcten
 *
 * Created on 2015年12月8日, 下午8:57
 */

#ifndef __WBT_SSL_H__
#define	__WBT_SSL_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
    
#include "../webit.h"

wbt_status wbt_ssl_init();
wbt_status wbt_ssl_exit();
wbt_status wbt_ssl_on_conn( wbt_event_t *ev );
wbt_status wbt_ssl_on_close( wbt_event_t *ev );

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_SSL_H__ */

