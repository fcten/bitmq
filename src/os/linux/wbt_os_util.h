/* 
 * File:   wbt_os_util.h
 * Author: Fcten
 *
 * Created on 2015年4月29日, 下午4:35
 */

#ifndef __WBT_OS_UTIL_H__
#define	__WBT_OS_UTIL_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <signal.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "wbt_setproctitle.h"
#include "wbt_terminal.h"
#include "wbt_process.h"    
    
#define wbt_inline inline

typedef int wbt_err_t;

#define WBT_EPERM         EPERM
#define WBT_ENOENT        ENOENT
#define WBT_ENOPATH       ENOENT
#define WBT_ESRCH         ESRCH
#define WBT_EINTR         EINTR
#define WBT_ECHILD        ECHILD
#define WBT_ENOMEM        ENOMEM
#define WBT_EACCES        EACCES
#define WBT_EBUSY         EBUSY
#define WBT_EEXIST        EEXIST
#define WBT_EEXIST_FILE   EEXIST
#define WBT_EXDEV         EXDEV
#define WBT_ENOTDIR       ENOTDIR
#define WBT_EISDIR        EISDIR
#define WBT_EINVAL        EINVAL
#define WBT_ENFILE        ENFILE
#define WBT_EMFILE        EMFILE
#define WBT_ENOSPC        ENOSPC
#define WBT_EPIPE         EPIPE
#define WBT_EINPROGRESS   EINPROGRESS
#define WBT_ENOPROTOOPT   ENOPROTOOPT
#define WBT_EOPNOTSUPP    EOPNOTSUPP
#define WBT_EADDRINUSE    EADDRINUSE
#define WBT_ECONNABORTED  ECONNABORTED
#define WBT_ECONNRESET    ECONNRESET
#define WBT_ENOTCONN      ENOTCONN
#define WBT_ETIMEDOUT     ETIMEDOUT
#define WBT_ECONNREFUSED  ECONNREFUSED
#define WBT_ENAMETOOLONG  ENAMETOOLONG
#define WBT_ENETDOWN      ENETDOWN
#define WBT_ENETUNREACH   ENETUNREACH
#define WBT_EHOSTDOWN     EHOSTDOWN
#define WBT_EHOSTUNREACH  EHOSTUNREACH
#define WBT_ENOSYS        ENOSYS
#define WBT_ECANCELED     ECANCELED
#define WBT_EILSEQ        EILSEQ
#define WBT_ENOMOREFILES  0
#define WBT_ELOOP         ELOOP
#define WBT_EBADF         EBADF

#if (WBT_HAVE_OPENAT)
#define WBT_EMLINK        EMLINK
#endif

#if (__hpux__)
#define WBT_EAGAIN        EWOULDBLOCK
#else
#define WBT_EAGAIN        EAGAIN
#endif


#define wbt_errno                  errno
#define wbt_socket_errno           errno
#define wbt_set_errno(err)         errno = err
#define wbt_set_socket_errno(err)  errno = err

int wbt_get_file_path_by_fd(int fd, void * buf, size_t buf_len);
int wbt_getopt(int argc,char * const argv[ ],const char * optstring);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_OS_UTIL_H__ */

