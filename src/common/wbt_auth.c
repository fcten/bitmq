/* 
 * File:   wbt_auth.c
 * Author: fcten
 *
 * Created on 2017年3月1日, 上午10:35
 */

#include <assert.h>

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "wbt_auth.h"
#include "wbt_module.h"
#include "wbt_config.h"
#include "wbt_log.h"
#include "wbt_base64.h"
#include "wbt_string.h"

static EVP_PKEY *key;

wbt_status static wbt_auth_load_key();
wbt_status wbt_auth_init();
wbt_status wbt_auth_exit();

wbt_module_t wbt_module_auth = {
    wbt_string("auth"),
    wbt_auth_init,
    wbt_auth_exit
};

wbt_status wbt_auth_init() {
    if( wbt_conf.auth != 2 ) {
        return WBT_OK;
    }
    
    if( wbt_auth_load_key() != WBT_OK ) {
        return WBT_ERROR;
    }
    
    return WBT_OK;
}

wbt_status wbt_auth_exit() {
    return WBT_OK;
}

wbt_status wbt_auth_verify(wbt_str_t *token, wbt_str_t *sign) {
    if(key == NULL) {
        return WBT_ERROR;
    }

    static wbt_str_t signature;
    if(signature.len == 0) {
        signature.len = EVP_PKEY_size(key);
        signature.str = (char *) wbt_malloc(signature.len);
    }

    if( wbt_base64_decode(&signature, sign) != WBT_OK ) {
        return WBT_ERROR;
    }
    
    EVP_MD_CTX *md_ctx = EVP_MD_CTX_create();
    if (md_ctx == NULL ) {
        return WBT_ERROR;
    }
    
    int result = -1;
    static const EVP_MD *md = NULL;
    if(md == NULL) {
        md = EVP_sha256();
    }
    
    do {
        if(!EVP_VerifyInit(md_ctx, md)) {
            break;
        }

        if(!EVP_VerifyUpdate(md_ctx, (unsigned char *)token->str, (unsigned int)token->len)) {
            break;
        }

        int ret = EVP_VerifyFinal(md_ctx, (unsigned char *)signature.str, (unsigned int)signature.len, key);
        if(ret < 0) {
            break;
        } else if(ret == 1) {
            result = 0;
        } else {
            result = -1;
        }
    } while(0);
    
    EVP_MD_CTX_destroy(md_ctx);
    
    if( result == 0 ) {
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}

static wbt_status wbt_auth_load_key() {
    int result = -1;
    
    if(key != NULL) {
        EVP_PKEY_free(key);
        key = NULL;
    }
    
    RSA* rsa = NULL;
    FILE *fp = NULL;

    do {
        if( wbt_conf.auth_key.len == 0 ) {
            break;
        }
        
        key = EVP_PKEY_new();
        assert(key != NULL);
        if(key == NULL) {
            wbt_log_add("EVP_PKEY_new failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
        
        fp = fopen(wbt_stdstr(&wbt_conf.auth_key), "rb");
        assert(fp != NULL);
        if( fp == NULL ) {
            wbt_log_add("can't open auth key file: %.*s\n", wbt_conf.auth_key.len, wbt_conf.auth_key.str);
            break; /* failed */
        }
        rsa = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL);
        fclose(fp);

        assert(rsa != NULL);
        if(rsa == NULL) {
            printf("PEM_read_RSAPublicKey failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        /* Set verifier key */
        int rc = EVP_PKEY_assign_RSA(key, RSAPublicKey_dup(rsa));
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_PKEY_assign_RSA failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
        
        result = 0;
        
    } while(0);
    
    if(rsa) {
        RSA_free(rsa);
        rsa = NULL;
    }

    if(result == 0) {
        return WBT_OK;
    } else {
        return WBT_ERROR;
    }
}