/* 
 * File:   wbt_list.h
 * Author: Fcten
 *
 * Created on 2015年2月15日, 上午9:24
 */

#ifndef __WBT_LIST_H__
#define	__WBT_LIST_H__

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct wbt_list_s {  
    struct list_s *next, *prev;  
} wbt_list_t;

/* 用于通过 wbt_list_t 指针获得完整结构体的指针
 */
#define wbt_list_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/* 用于 wbt_list_t 初始化，必须先声明再调用
 */
#define wbt_list_init(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

#ifdef	__cplusplus
}
#endif

#endif	/* __WBT_LIST_H__ */

