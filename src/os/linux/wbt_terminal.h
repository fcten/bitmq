/* 
 * File:   wbt_terminal.h
 * Author: Fcten
 *
 * Created on 2015年4月17日, 上午10:12
 */

#ifndef WBT_TERMINAL_H
#define	WBT_TERMINAL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include<sys/types.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<termios.h>

#include "../../webit.h"

void wbt_term_get_size();

#ifdef	__cplusplus
}
#endif

#endif	/* WBT_TERMINAL_H */

