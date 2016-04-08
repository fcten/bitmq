/* 
 * File:   wbt_gzip.h
 * Author: fcten
 *
 * Created on 2016年2月10日, 下午9:33
 */

#ifndef WBT_GZIP_H
#define	WBT_GZIP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
    
#ifndef WIN32

#include <zlib.h>

/* Compress gzip data */
int wbt_gzip_compress(Bytef *data, uLong ndata, Bytef *zdata, uLong *nzdata);

/* HTTP gzip decompress */
int wbt_gzip_decompress(Byte *zdata, uLong nzdata, Byte *data, uLong *ndata);

#else

#include "../webit.h"

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

typedef unsigned char  Byte;  /* 8 bits */
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

typedef Byte Bytef;

static wbt_inline int wbt_gzip_compress(Bytef *data, uLong ndata, Bytef *zdata, uLong *nzdata) {
    return Z_MEM_ERROR;
}

static wbt_inline int wbt_gzip_decompress(Byte *zdata, uLong nzdata, Byte *data, uLong *ndata) {
    return Z_MEM_ERROR;
}

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_GZIP_H */

