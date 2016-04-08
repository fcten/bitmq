/* 
 * File:   wbt_os_util.c
 * Author: Fcten
 *
 * Created on 2015年4月29日, 下午4:35
 */

#include "../../webit.h"
#include "../../common/wbt_memory.h"
#include "wbt_os_util.h"

int wbt_nonblocking(wbt_socket_t s) {
    unsigned long nb = 1;
    return ioctlsocket(s, FIONBIO, &nb);
}

int wbt_blocking(wbt_socket_t s) {
    unsigned long nb = 0;
    return ioctlsocket(s, FIONBIO, &nb);
}

/* 尝试对指定文件加锁
 * 如果加锁失败，进程将会睡眠，直到加锁成功
 */
int wbt_trylock_fd(wbt_fd_t fd) {
	OVERLAPPED oapped;
	if (LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK, 0, 200, 0, &oapped)) {
		return 0;
	}

	return -1;
}

/* 尝试对指定文件加锁
 * 如果加锁失败，立刻出错返回
 */
int wbt_lock_fd(wbt_fd_t fd) {
	OVERLAPPED oapped;
	if (LockFileEx(fd, LOCKFILE_FAIL_IMMEDIATELY | LOCKFILE_EXCLUSIVE_LOCK, 0, 200, 0, &oapped)) {
		return 0;
	}

	return -1;
}

int wbt_unlock_fd(wbt_fd_t fd) {
	OVERLAPPED oapped;
	if (UnlockFileEx(fd, 0, 200, 0, &oapped)) {
		return 0;
	}

	return -1;
}

wbt_fd_t wbt_lock_create(const char *name) {
	return CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
}

wbt_fd_t wbt_open_logfile(const char *name) {
	return CreateFile(name, GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

wbt_fd_t wbt_open_datafile(const char *name) {
	return CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

wbt_fd_t wbt_open_tmpfile(const char *name) {
	return CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS | TRUNCATE_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
}