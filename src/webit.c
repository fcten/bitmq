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

extern char **environ;
char *last;

void initProcTitle(int argc, char **argv)
{
    size_t size = 0;
    int i;
    for (i = 0; environ[i]; ++i) {
        size += strlen(environ[i])+1; 
    }   
 
    char *raw = malloc(size*sizeof(char));
    for (i = 0; environ[i]; ++i) {
        memcpy(raw, environ[i], strlen(environ[i]) + 1); 
        environ[i] = raw;
        raw += strlen(environ[i]) + 1;
    }   
 
    last = argv[0];
    for (i = 0; i < argc; ++i) {
        last += strlen(argv[i]) + 1;   
    }   
    for (i = 0; environ[i]; ++i) {
        last += strlen(environ[i]) + 1;
    }   
}
 
void setProcTitle(int argc, char **argv, const char *title)
{
    argv[1] = 0;
    char *p = argv[0];
    memset(p, 0x00, last - p); 
    strncpy(p, title, last - p); 
}
 
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
    
    initProcTitle(argc, argv);
    
    while(1) {
        childpid = fork();

        if ( -1 == childpid ) {
            perror( "fork()" );
            exit( EXIT_FAILURE );
        } else if ( 0 == childpid ) {
            /* In child process */
            setProcTitle(argc, argv, "Webit: worker process");
            break;
        } else {
            /* In parent */
            setProcTitle(argc, argv, "Webit: master process");
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

