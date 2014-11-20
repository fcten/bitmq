/* 
 * File:   wbt_file.h
 * Author: Fcten
 *
 * Created on 2014年11月12日, 上午10:44
 */

#ifndef __WBT_FILE_H__
#define	__WBT_FILE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include "../webit.h"

/* 在读取文件内容时，每一次请求都进行一次打开和关闭是不必要的开销。
 * 打开文件之后将句柄缓存起来，下一次打开相同文件时可以直接返回句柄。
 * 这样对于每个请求的系统调用次数就可以减少到 1 次 (sendfile)。
 * 
 * 在这里采取的策略是:
 * 1、缓存所有被访问的文件句柄。
 * 2、维护一个红黑树用于快速取得已打开的文件句柄。
 * 3、如果打开的文件数量超过阈值，移除最久未访问的文件。
 */
typedef struct wbt_file_s {
    int fd;                 /* 文件句柄 */
    int refer;              /* 文件引用计数 */
    size_t size;            /* 文件大小 */
    off_t offset;           /* 已发送数据偏移量 */
} wbt_file_t;

wbt_status wbt_file_init();

wbt_file_t * wbt_file_open( wbt_str_t * file_path );

wbt_status wbt_file_close( wbt_str_t * file_path );

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_FILE_H__ */

