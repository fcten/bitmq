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
#include <grp.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include "webit.h"
#include "common/wbt_string.h"
#include "common/wbt_memory.h"
#include "common/wbt_log.h"
#include "common/wbt_module.h"
#include "common/wbt_rbtree.h"
#include "common/wbt_config.h"

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

int wating_to_exit = 0;

void wbt_signal(int signal) {
    wbt_log_add("received singal: %d\n", signal);
    
    switch( signal ) {
        case SIGCHLD:
            /* 仅父进程：子进程退出，重新创建一个子进程 */
            break;
        case SIGTERM:
            /* 父进程：停止所有子进程并退出 */
            /* 子进程：停止事件循环并退出 */
            wating_to_exit = 1;

            break;
            /* 仅父进程：reload 信号 */
        case SIGINT:
            /* 仅调试模式，退出 */
            wating_to_exit = 1;
            break;
    }
}

void wbt_worker_process() {    
    /* 限制可以访问的目录
     * 这个操作会导致 daemon() 不能正常运行
     * 所以检测 root 目录是否存在应该添加到 http 模块的初始化方法中
     */
    const char * wwwroot = wbt_conf_get("root");
    if( chroot( wwwroot ) != 0 ) {
        wbt_log_add("%s not exists.\n", wwwroot);
        return;
    } else {
        wbt_log_add("Root path: %s\n", wwwroot);
    }
    
    wbt_log_add("Webit startup (pid: %d)\n", getpid());

    /* 降低 worker 进程的权限 */
    if (geteuid() == 0) {
        if (setgid(33) == -1) {
            wbt_log_debug("setgid(%d) failed", 33)
            return;
        }

        if (initgroups("www-data", 33) == -1) {
            wbt_log_debug("initgroups(www-data, %d) failed", 33);
            return;
        }

        if (setuid(33) == -1) {
            wbt_log_debug("setuid(%d) failed", 33);
            return;
        }
    }

    /* 进入事件循环 */
    wbt_event_dispatch();
}

void wbt_master_process() {
    /* 设置需要监听的信号(后台模式) */
    struct sigaction act;
    sigset_t set;

    act.sa_handler = wbt_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        wbt_log_add("sigprocmask() failed");
    }

    sigemptyset(&set);
    
    sigaction(SIGCHLD, &act, NULL); /* 子进程退出 */
    sigaction(SIGTERM, &act, NULL); /* 命令 Webit 退出 */
    /* 自定义的 reload 信号 */

    while(!wating_to_exit) {
        sigsuspend(&set);
    }
}

pid_t wbt_fork_process() {
    /* 创建运行实例 */
    return fork();
}

int wbt_debug_on = 0;

int main(int argc, char** argv) {
    /* 初始化所有组件 */
    wbt_module_init();

    /* 设置程序允许打开的最大文件句柄数 */
    struct rlimit rlim;
    rlim.rlim_cur = 65535;
    rlim.rlim_max = 65535;
    setrlimit(RLIMIT_NOFILE, &rlim);

    /* 接下来的 chroot 会导致程序无法访问 /etc/timezone
     * TODO 读取 /etc/timezone 的内容并保存
     */
    if( setenv("TZ", "Asia/Shanghai", 1) != 0 ) {
        perror("setenv");
        return;
    }
    tzset();
    
    /* 判断是否进入 debug 模式 */
    wbt_str_t * run_mode = (wbt_str_t *)wbt_conf_get_v("run_mode");
    wbt_str_t debug_on = wbt_string("debug");
    if( run_mode != NULL && wbt_strcmp2( run_mode, &debug_on ) == 0 ) {
        wbt_debug_on = 1;
    }

    if( wbt_debug_on ) {
        /* 设置需要监听的信号(前台模式) */
        signal(SIGTERM, wbt_signal);
        signal(SIGINT, wbt_signal);
        
        wbt_worker_process();
    } else {
        /* 转入后台运行 */
        if( daemon(1,0) < 0 ) {
            perror("error daemon");  
            return 1;
        }

        initProcTitle(argc, argv);

        /* 创建运行实例 */
        pid_t childpid;

        childpid = wbt_fork_process();

        if ( -1 == childpid ) {
            return 1;
        } else if ( 0 == childpid ) {
            /* In child process */
            setProcTitle(argc, argv, "Webit: worker process");
            wbt_worker_process();
        } else {
            /* In parent */
            setProcTitle(argc, argv, "Webit: master process (default)");
            wbt_master_process();
        }
    }

    /* 卸载所有模块 */
    wbt_module_exit();

    wbt_log_add("Webit exit (pid: %d)\n", getpid());

    return 0;
}

