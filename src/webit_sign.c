/* 
 * 计算并生成授权信息
 */

#include <stdio.h>
#include <assert.h>

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "webit.h"
#include "common/wbt_base64.h"
#include "common/wbt_auth.h"

static EVP_PKEY* key;

int load_keys(const char *key_path) {
    int result = -1;
    
    RSA* rsa = NULL;
    
    do {
        key = EVP_PKEY_new();
        assert(key != NULL);
        if(key == NULL) {
            printf("EVP_PKEY_new failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
        
        //rsa = RSA_generate_key(2048, RSA_F4, NULL, NULL);
        FILE* fp = fopen(key_path, "rb");
        assert(fp != NULL);
        if( fp == NULL ) {
            printf("can't open key file: %s\n", key_path);
            break; /* failed */
        }
        rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
        fclose(fp);
        assert(rsa != NULL);
        if(rsa == NULL) {
            printf("RSA_generate_key failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
        
        /* Set signing key */
        int rc = EVP_PKEY_assign_RSA(key, RSAPrivateKey_dup(rsa));
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_PKEY_assign_RSA (1) failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        result = 0;
        
    } while(0);
    
    if(rsa) {
        RSA_free(rsa);
        rsa = NULL;
    }
    
    return !!result;
}

int main(int argc, char **argv) {
    
    


    return 0;
}
