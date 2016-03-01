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
    /* 文件句柄 */
    int fd;
    /* 文件引用计数 */
    int refer;
    /* 如果文件被读取到内存中，则在 ptr 中保存文件内容的指针
     * 否则的话，ptr 的值保持 NULL
     */
    char *ptr;
    /* 文件大小 */
    size_t size;
    /* 如果使用 gzip，则 gzip_ptr 中保存压缩后的文件内容 */
    char *gzip_ptr;
    /* 压缩后的文件大小 */
    size_t gzip_size;
    /* 文件最后修改时间 */
    time_t last_modified;
    /* 文件最后被访问时间 */
    time_t last_use_mtime;
} wbt_file_t;

wbt_status wbt_file_init();
wbt_status wbt_file_exit();

wbt_file_t * wbt_file_open(wbt_str_t * file_path);
wbt_status wbt_file_close(wbt_file_t * file);

ssize_t wbt_file_read(wbt_file_t *file);
wbt_status wbt_file_compress(wbt_file_t *file);

ssize_t wbt_file_size(wbt_str_t * file_path);

wbt_status wbt_trylock_fd(int fd);
wbt_status wbt_lock_fd(int fd);
wbt_status wbt_unlock_fd(int fd);
int wbt_lock_create(const char *name);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_FILE_H__ */

