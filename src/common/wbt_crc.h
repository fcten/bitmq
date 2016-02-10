/* 
 * File:   wbt_crc.h
 * Author: fcten
 *
 * Created on 2016年2月10日, 下午9:03
 */

#ifndef WBT_CRC_H
#define	WBT_CRC_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

uint32_t wbt_crc32( const unsigned char *buf, size_t size );

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_CRC_H */

