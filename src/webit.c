/* 
 * File:   webit.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午2:10
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>  
#include <sys/wait.h>
#include <sys/resource.h>

#include "webit.h"
#include "common/wbt_string.h"
#include "common/wbt_memory.h"
#include "common/wbt_log.h"

void wbt_null() {}
/*
 * 
 */
int main(int argc, char** argv) {
    pid_t childpid;
    int status;

    if( daemon(1,0) < 0 ) {
        perror("error daemon");  
        return 1;
    }
    
    while(1) {
        childpid = fork();

        if ( -1 == childpid ) {
            perror( "fork()" );
            exit( EXIT_FAILURE );
        } else if ( 0 == childpid ) {
            /* In child process */
            //setproctitle("Webit: worker process");
            break;
        } else {
            /* In parent */
            //setproctitle("Webit: master process");
            waitpid( childpid, &status, 0 );
        }
    }

    wbt_mem_t p;
    wbt_str_t s;

    wbt_malloc(&p, 20);
    wbt_memset(&p, 0);
    s = wbt_sprintf(&p, "Webit startup %d.", getpid());
    wbt_log_write(s, stderr);
    wbt_free(&p);
    
    wbt_log_debug("123");

    if( wbt_event_init() != WBT_OK ) {
        return 1;
    }

    if( wbt_conn_init() != WBT_OK ) {
        return 1;
    }
    
    /* 设置程序允许打开的最大文件句柄数 */
    struct rlimit rlim;
    rlim.rlim_cur = 65535;
    rlim.rlim_max = 65535;
    setrlimit(RLIMIT_NOFILE, &rlim);
    
    /* 设置需要监听的信号 */
    /*sigaction(SIGCHLD, NULL, NULL);
    sigaction(SIGALRM, NULL, NULL);
    sigaction(SIGIO, NULL, NULL);
    sigaction(SIGINT, NULL, NULL);*/
    signal(SIGINT, wbt_null);
    
    wbt_event_dispatch();

    wbt_conn_cleanup();

    wbt_malloc(&p, 20);
    wbt_memset(&p, 0);
    s = wbt_sprintf(&p, "Webit exit");
    wbt_log_write(s, stderr);
    wbt_free(&p);

    return 0;
}

