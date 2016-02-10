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
#include <zlib.h>

/* Compress gzip data */
int wbt_gzip_compress(Bytef *data, uLong ndata, Bytef *zdata, uLong *nzdata);

/* HTTP gzip decompress */
int wbt_gzip_decompress(Byte *zdata, uLong nzdata, Byte *data, uLong *ndata);

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_GZIP_H */

