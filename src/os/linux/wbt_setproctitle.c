/* 
 * File:   wbt_setproctitle.c
 * Author: Fcten
 *
 * Created on 2015年4月20日, 下午1:08
 */

#include "wbt_setproctitle.h"
#include "../../common/wbt_string.h"
#include "../../common/wbt_memory.h"

extern int wbt_argc;
extern char **wbt_os_argv;
extern char **wbt_os_environ;

char *wbt_argv_last;

int wbt_init_proc_title() {
    size_t size = 0;
    int i;
    char * p;

    for (i = 0; wbt_os_environ[i]; ++i) {
        size += strlen(wbt_os_environ[i]) + 1;
    }
 
    if( ( p = wbt_malloc(size*sizeof(char)) ) == NULL ) {
        return -1;
    }
    
    wbt_argv_last = wbt_os_argv[0];

    for (i = 0; wbt_os_argv[i]; i++) {
        if (wbt_argv_last == wbt_os_argv[i]) {
            wbt_argv_last = wbt_os_argv[i] + strlen(wbt_os_argv[i]) + 1;
        }
    }
    
    for (i = 0; wbt_os_environ[i]; i++) {
        if (wbt_argv_last == wbt_os_environ[i]) {

            size = wbt_strlen(wbt_os_environ[i]) + 1;
            wbt_argv_last = wbt_os_environ[i] + size;

            wbt_memcpy(p, wbt_os_environ[i], size);
            wbt_os_environ[i] = p;
            p += size;
        }
    }

    wbt_argv_last --;

    return 0;
}
 
void wbt_set_proc_title(const char *title) {
    wbt_os_argv[1] = 0;

    wbt_memcpy( wbt_os_argv[0], title, wbt_strlen(title)+1 );

    *wbt_argv_last = '\0';
}