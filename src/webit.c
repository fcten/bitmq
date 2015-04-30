/* 
 * File:   webit.c
 * Author: Fcten
 *
 * Created on 2014年8月20日, 下午2:10
 */

#include <stdio.h>
#include <stdlib.h>
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
#include "os/linux/wbt_sigsegv.h"
#include "os/linux/wbt_setproctitle.h"
#include "os/linux/wbt_os_util.h"

int wbt_argc;
char** wbt_argv;

int wating_to_exit = 0;

void wbt_signal(int signo, siginfo_t *info, void *context) {
    wbt_log_debug("received singal: %d", signo);

    pid_t pid;
    int   stat;

    switch( signo ) {
        case SIGCHLD:
            /* 仅父进程：子进程退出，从列表中移除
             * 如果同时有大量进程退出，SIGCHLD 信号可能会丢失，
             * 所以不能使只根据 info 中的 pid 判断
             */
            while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
               wbt_proc_remove(pid);
            }

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
    /* 设置进程标题 */
    if( !wbt_conf.run_mode ) {
        wbt_set_proc_title("webit: worker process");
    }

    /* 设置需要监听的信号(后台模式) */
    struct sigaction act;
    sigset_t set;

    act.sa_sigaction = wbt_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGTERM);

    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
        wbt_log_add("sigprocmask() failed\n");
    }

    sigaction(SIGINT, &act, NULL); /* 退出信号 */
    sigaction(SIGTERM, &act, NULL); /* 退出信号 */
    
    wbt_log_add("Webit startup (pid: %d)\n", getpid());

    /* 降低 worker 进程的权限 
    const char * user = wbt_conf_get("user");
    if ( user != NULL && geteuid() == 0 ) {
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
    } */

    /* 进入事件循环 */
    wbt_event_dispatch();
    
    wbt_exit(0);
}

void wbt_master_process() {
    /* 设置需要监听的信号(后台模式) */
    struct sigaction act;
    sigset_t set;

    act.sa_sigaction = wbt_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        wbt_log_add("sigprocmask() failed\n");
    }

    sigemptyset(&set);
    
    sigaction(SIGCHLD, &act, NULL); /* 子进程退出 */
    sigaction(SIGTERM, &act, NULL); /* 命令 Webit 退出 */
    /* 自定义的 reload 信号 */

    time_t prev_time;
    int count = 0;
    while(!wating_to_exit) {
        /* 防止 worker process 出错导致 fork 炸弹 */
        wbt_time_update();
        if( cur_mtime - prev_time <= 1000 ) {
            if( ( count ++ ) > 9 ) {
                wbt_log_add("try to fork child too fast\n");
                /* 触发限制后，每 5s 尝试一次 
                 * 由于阻塞了相关信号，这会导致信号处理出现延迟
                 */
                sleep(5);
            }
        } else {
            count = 0;
        }
        prev_time = cur_mtime;
        
        /* 创建子进程直至指定数量 */
        wbt_proc_create(wbt_worker_process, wbt_conf.process);
        
        sigsuspend(&set);
    }

    /* 结束所有子进程 */
    pid_t child;
    while( ( child = wbt_proc_pop() ) != 0 ) {
        kill( child, SIGTERM );
    }
    
    wbt_exit(0);
}

void wbt_exit(int exit_code) {
    wating_to_exit = 1;

    /* 卸载所有模块 */
    wbt_module_exit();

    wbt_log_add("Webit exit (pid: %d)\n", getpid());
    wbt_log_print("\n\nWebit exit (pid: %d)\n", getpid());
    
    exit(exit_code);
}

extern wbt_mem_t wbt_log_buf;

int main(int argc, char** argv) {
    /* 保存传入参数 */
    wbt_argc = argc;
    wbt_argv = argv;

    /* 初始化日至输出缓冲 */
    if( wbt_malloc( &wbt_log_buf, 1024 ) != WBT_OK ) {        
        return 1;
    }
    
    /* 更新终端尺寸 */
    wbt_term_update_size();
    
    /* 解析传入参数 */
    int ch;
    opterr = 0;
    while( ( ch = wbt_getopt(argc,argv,"c:hs:tv") ) != -1 ) {
        switch(ch) {
            case 'v':
                wbt_log_print( "webit version: webit/" WBT_VERSION "\n" );
                return 0;
            case 'h':
                  wbt_log_print(
                      "\nOptions:\n"
                      "  -h               : this help\n"
                      "  -v               : show version and exit\n"
                      "  -s [stop|reload] : send signal to a master process\n"
                      "  -t [config-file] : test configuration and exit\n"
                      "  -c [config-file] : use the specified configuration\n\n");
                return 0;
            case 'c':
                if( wbt_conf_set_file(optarg) != WBT_OK ) {
                    return 1;
                }
                break;
            case '?':
                if( optopt == 'c' || optopt == 's' || optopt == 't' ) {
                    wbt_log_print("option: -%c required parameters\n",optopt);
                } else {
                    wbt_log_print("invalid option: -%c\n",optopt);
                }
                return 1;
            default:
                wbt_log_print("option: -%c not yet implemented\n",ch);
                return 1;
        }
    }
    
    setup_sigsegv();

    wbt_init_proc_title();
    
    /* 初始化所有组件 */
    wbt_log_print( "webit version: webit/" WBT_VERSION "\n" );
    if( wbt_module_init() != WBT_OK ) {
        wbt_log_print( "\n\n" );
        return 1;
    }

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

    if( !wbt_conf.run_mode ) {
        wbt_log_print( "\n\nwebit is now running in the background.\n\n" );

        /* 转入后台运行 */
        if( daemon(1,0) < 0 ) {
            perror("error daemon");  
            return 1;
        }
    } else {
        wbt_log_print( "\n\nwebit is now running.\n\n" );
    }

    /* 限制可以访问的目录
     * 这个操作会导致 daemon() 不能正常运行
     */
    const char * wwwroot = wbt_stdstr(&wbt_conf.root);
    if( chroot( wwwroot ) != 0 ) {
        wbt_log_add("%s not exists.\n", wwwroot);
        return;
    } else {
        wbt_log_add("Root path: %s\n", wwwroot);
    }

    if( !wbt_conf.run_mode ) {
        wbt_master_process();
    } else {
        wbt_worker_process();
    }
    
    return 0;
}

