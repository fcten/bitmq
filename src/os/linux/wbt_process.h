/* 
 * File:   wbt_process.h
 * Author: Fcten
 *
 * Created on 2015年2月9日, 上午9:18
 */

#ifndef __WBT_PROCESS_H__
#define	__WBT_PROCESS_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <unistd.h>

#define WBT_MAX_PROCESS 1024

typedef struct wbt_proc_s {
    pid_t pid;
} wbt_proc_t;

int   wbt_proc_push(pid_t pid);
pid_t wbt_proc_pop();
int   wbt_proc_fork( void (*proc)() );
int   wbt_proc_create( void (*proc)(), int num );
void  wbt_proc_remove(pid_t pid);

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_PROCESS_H__ */

