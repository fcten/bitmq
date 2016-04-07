/* 
 * File:   wbt_gzip.c
 * Author: fcten
 *
 * Created on 2016年2月10日, 下午9:33
 */

#include "wbt_gzip.h"
#include "wbt_memory.h"

#ifndef WIN32

/* 使用自定义的内存分配策略 */  
void *wbt_gzip_alloc OF((void *, unsigned, unsigned));  
void wbt_gzip_free OF((void *, void *));

static alloc_func zalloc = wbt_gzip_alloc;  
static free_func zfree = wbt_gzip_free;  

void *wbt_gzip_alloc(void *q, unsigned n, unsigned m) {  
    q = Z_NULL;  
    return wbt_calloc(n*m);
}  
  
void wbt_gzip_free(void *q, void *p) {
    q = Z_NULL;  
    wbt_free(p);
}

/* Compress gzip data */
int wbt_gzip_compress(Bytef *data, uLong ndata, Bytef *zdata, uLong *nzdata) {
    z_stream c_stream;
    int err = 0;

    if(data && ndata > 0)
    {
        c_stream.zalloc = zalloc;
        c_stream.zfree = zfree;
        c_stream.opaque = (voidpf)0;
        if((err=deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
            MAX_WBITS+16, 8, Z_DEFAULT_STRATEGY)) != Z_OK) return err;
        c_stream.next_in  = data;
        c_stream.avail_in  = ndata;
        c_stream.next_out = zdata;
        c_stream.avail_out  = *nzdata;
        while (c_stream.avail_in != 0 && c_stream.total_out < *nzdata) 
        {
            if((err = deflate(&c_stream, Z_NO_FLUSH)) != Z_OK) {
                deflateEnd(&c_stream);
                return err;
            }
        }
        if(c_stream.avail_in != 0) return c_stream.avail_in;
        for (;;) {
            if((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END) break;
            if(err != Z_OK) {
                deflateEnd(&c_stream);
                return err;
            }
        }
        if((err = deflateEnd(&c_stream)) != Z_OK) {
            deflateEnd(&c_stream);
            return err;
        }
        *nzdata = c_stream.total_out;

        deflateEnd(&c_stream);
        return Z_OK;
    }
    return -1;
}

/* HTTP gzip decompress */
int wbt_gzip_decompress(Byte *zdata, uLong nzdata, Byte *data, uLong *ndata) {
    int err = 0;
    z_stream d_stream = {0}; /* decompression stream */
    static char dummy_head[2] = 
    {
        0x8 + 0x7 * 0x10,
        (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };
    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (voidpf)0;
    d_stream.next_in  = zdata;
    d_stream.avail_in = 0;
    d_stream.next_out = data;
    if((err = inflateInit2(&d_stream, 47)) != Z_OK) return err;
    while ( 1 ) {
        if( !( d_stream.total_out < *ndata && d_stream.total_in < nzdata ) ) {
            deflateEnd(&d_stream);
            return Z_BUF_ERROR;
        }
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        if((err = inflate(&d_stream, Z_NO_FLUSH)) == Z_STREAM_END) break;
        if(err != Z_OK )
        {
            if(err == Z_DATA_ERROR)
            {
                d_stream.next_in = (Bytef*) dummy_head;
                d_stream.avail_in = sizeof(dummy_head);
                if((err = inflate(&d_stream, Z_NO_FLUSH)) != Z_OK) 
                {
                    deflateEnd(&d_stream);
                    return err;
                }
            } else {
                deflateEnd(&d_stream);
                return err;
            }
        }
    }
    if((err = inflateEnd(&d_stream)) != Z_OK) {
        deflateEnd(&d_stream);
        return err;
    }
    *ndata = d_stream.total_out;
    
    deflateEnd(&d_stream);
    return Z_OK;
}

#endif