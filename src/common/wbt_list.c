/* 
 * File:   wbt_list.c
 * Author: Fcten
 *
 * Created on 2015年2月15日, 上午9:24
 */

#include "wbt_list.h"

/* 用于操作双向循环链表，也可以当作单向链表使用
 */

static inline void __wbt_list_add(wbt_list_t *new, wbt_list_t *prev, wbt_list_t *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void __wbt_list_del(wbt_list_t *prev, wbt_list_t *next) {
    next->prev = prev;
    prev->next = next;
}
  
static inline void wbt_list_add(wbt_list_t *new, wbt_list_t *head) {
    __wbt_list_add(new, head, head->next);
}

static inline void wbt_list_add_tail(wbt_list_t *new, wbt_list_t *head) {
    __wbt_list_add(new, head->prev, head);
}  

static inline void list_del(wbt_list_t *entry) {
    __wbt_list_del(entry->prev, entry->next);
}
