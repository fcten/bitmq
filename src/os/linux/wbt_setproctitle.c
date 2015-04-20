/* 
 * File:   wbt_setproctitle.c
 * Author: Fcten
 *
 * Created on 2015年4月20日, 下午1:08
 */

#include "wbt_setproctitle.h"

extern int wbt_argc;
extern char** wbt_argv;
extern char **environ;

char *wbt_argv_last;

wbt_status wbt_init_proc_title() {
    size_t size = 0;
    int i;
    char * p;

    for (i = 0; environ[i]; ++i) {
        size += strlen(environ[i]) + 1;
    }
 
    if( ( p = malloc(size*sizeof(char)) ) == NULL ) {
        return WBT_ERROR;
    }
    
    wbt_argv_last = wbt_argv[0];

    for (i = 0; wbt_argv[i]; i++) {
        if (wbt_argv_last == wbt_argv[i]) {
            wbt_argv_last = wbt_argv[i] + strlen(wbt_argv[i]) + 1;
        }
    }
    
    for (i = 0; environ[i]; i++) {
        if (wbt_argv_last == environ[i]) {

            size = wbt_strlen(environ[i]) + 1;
            wbt_argv_last = environ[i] + size;

            memcpy(p, (u_char *) environ[i], size);
            environ[i] = (char *) p;
            p += size;
        }
    }

    wbt_argv_last --;

    return WBT_OK;
}
 
void wbt_set_proc_title(const char *title) {
    wbt_argv[1] = 0;
    char * p;
    p = memcpy((u_char *) wbt_argv[0], (u_char *) "Webit: ",
	                    wbt_argv_last - wbt_argv[0]);
    p = memcpy(p, (u_char *) title, wbt_argv_last - (char *) p);

    if (wbt_argv_last - (char *) p) {
        memset(p, '\0', wbt_argv_last - (char *) p);
    }
}