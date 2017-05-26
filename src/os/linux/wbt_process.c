/* 
 * File:   wbt_process.c
 * Author: Fcten
 *
 * Created on 2015年2月9日, 上午9:18
 */

#include "../../webit.h"
#include "wbt_process.h"

int wbt_proc_child_num = -1;
wbt_proc_t wbt_proc_stack[WBT_MAX_PROCESS];

int wbt_proc_push(pid_t pid) {
    if( wbt_proc_child_num + 1 >= WBT_MAX_PROCESS ) {
        return -1;
    }

    wbt_proc_stack[++ wbt_proc_child_num].pid = pid;
    
    return 0;
}

pid_t wbt_proc_pop() {
    if( wbt_proc_child_num < 0 ) {
        return 0;
    }
    
    return wbt_proc_stack[wbt_proc_child_num --].pid;
}

int wbt_proc_fork( void (*proc)() ) {
    pid_t childpid;

    childpid = fork();

    if ( childpid == -1 ) {
        return -1;
    } else if ( childpid == 0 ) {
        /* In child process */
        /* 这个时候信号还在阻塞中，需要重新打开 */
        proc();
        wbt_exit(0);
        return 0;
    } else {
        /* In parent */
        wbt_proc_push(childpid);
        
        return 0;
    }
}

int wbt_proc_create( void (*proc)(), int num ) {
    int i, cur_proc_num = wbt_proc_child_num + 1;
    for(i = cur_proc_num ; i < num ; i ++) {
        if( wbt_proc_fork(proc) == -1 ) {
            return -1;
        }
    }
    return 0;
}

void wbt_proc_remove(pid_t pid) {
    int i;
    for( i = 0 ; i <= wbt_proc_child_num ; i ++ ) {
        if( wbt_proc_stack[i].pid == pid ) {
            /* 用最后一个元素覆盖，并使子进程数量减一 */
            wbt_proc_stack[i] = wbt_proc_stack[wbt_proc_child_num --];
            break;
        }
    }
}
