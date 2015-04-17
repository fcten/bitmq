/* 
 * File:   wbt_terminal.c
 * Author: Fcten
 *
 * Created on 2015年4月17日, 上午10:12
 */

#include "wbt_terminal.h"

int wbt_terminal_width = 80;
int wbt_terminal_height = 25;

/*获得终端宽度*/  
void wbt_term_get_size() {
    struct winsize wbuf;
    char *tp;
    if( isatty(STDOUT_FILENO) ) {  
        if( ioctl(STDOUT_FILENO, TIOCGWINSZ, &wbuf) == -1 ) {  
            wbuf.ws_col = wbuf.ws_row = 0;
        }
        if( wbuf.ws_col > 0 ) {
            wbt_terminal_width = wbuf.ws_col;
        } else {
            if( tp = getenv("COLUMNS") )
                wbt_terminal_width = atoi( tp );
        }
        if( wbuf.ws_row > 0 ) {
            wbt_terminal_height = wbuf.ws_row;
        } else {
            if( tp = getenv("LINES") )
                wbt_terminal_height = atoi( tp );
        }
    }
 }  