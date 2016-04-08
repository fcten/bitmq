/* 
 * File:   wbt_os_util.c
 * Author: Fcten
 *
 * Created on 2015年4月29日, 下午4:35
 */

#include "../../webit.h"
#include "../../common/wbt_memory.h"
#include "wbt_os_util.h"

int wbt_get_file_path_by_fd(int fd, void * buf, size_t buf_len) {
    if ( fd <= 0 ) {  
        return -1;  
    }  

    char tmp[32] = {'\0'};
    snprintf(tmp, sizeof(tmp), "/proc/self/fd/%d", fd);

    return readlink(tmp, buf, buf_len);
}

int wbt_getopt(int argc,char * const argv[ ],const char * optstring) {
    return getopt(argc,argv,optstring); 
}


int wbt_get_self(void * buf, size_t buf_len) {
    wbt_memset(buf, 0, buf_len);

    return readlink("/proc/self/exe", buf, buf_len);
}

int wbt_nonblocking(wbt_socket_t s) {
    int nb = 1;
    return ioctl(s, FIONBIO, &nb);
}


int wbt_blocking(wbt_socket_t s) {
    int nb = 0;
    return ioctl(s, FIONBIO, &nb);
}

/* 尝试对指定文件加锁
 * 如果加锁失败，进程将会睡眠，直到加锁成功
 */
int wbt_trylock_fd(wbt_fd_t fd) {
    struct flock  fl;

    memset(&fl, 0, sizeof(struct flock));
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return -1;
    }

    return 0;
}

/* 尝试对指定文件加锁
 * 如果加锁失败，立刻出错返回
 */
int wbt_lock_fd(wbt_fd_t fd) {
    struct flock  fl;

    memset(&fl, 0, sizeof(struct flock));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        return -1;
    }

    return 0;
}

int wbt_unlock_fd(wbt_fd_t fd) {
    struct flock  fl;

    memset(&fl, 0, sizeof(struct flock));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return -1;
    }

    return 0;
}

wbt_fd_t wbt_lock_create( const char *name ) {
    return open( name, O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU );
}

wbt_fd_t wbt_open_logfile(const char *name) {
    return open(name, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0777);
}

ssize_t wbt_read_file(wbt_fd_t fd, void *buf, size_t count, off_t offset) {
    return pread(fd, buf, count, offset);
}
