/* 
 * File:   libjson.h
 * Author: Fcten
 *
 * Created on 2015年5月6日, 上午10:24
 */

#ifndef __LIBJSON_H__
#define	__LIBJSON_H__

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	NULL
#define	NULL 0
#endif

#include <stdio.h>
#include <stdlib.h> // malloc & free
#include <memory.h> // memset
#include <assert.h> // assert

typedef enum {
    JSON_NONE,
    JSON_STRING,
    JSON_INTEGER,
    JSON_DOUBLE,
    JSON_LONGLONG,
    JSON_FLOAT,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
} json_type_t;

/* 64 位系统下占用 32 字节，32 位系统下占用 20 字节 */
typedef struct json_object_s {
    /* Bugfix: gcc 中 enum 变量为无符号，但 vs 中 enum 变量为有符号，*/
    /* 所以我们不能使用 json_type_t，必须将其声明为无符号类型         */
    size_t object_type : 4; /* 该属性只能是 JSON_OBJECT 或者 JSON_ARRAY */
    size_t value_type : 4;
    
#if (defined __x86_64__) || (defined _WIN64)
    /* 64 */
    size_t key_len:18;      // 64 位下一般不需要考虑长度不够的问题了
    size_t value_len:38;
#else
    /* 32 */
    size_t key_len:8;       // key 最长 255 字节
    size_t value_len:16;    // value 最长 65535 字节
#endif
    
    union {
        double d;   /* TODO double to string 可参考 Grisu 提高性能 */
        float f;
        long long l;
        int i;
        char * s;
        struct json_object_s * p;
    } value;
    
    /* 该属性只被 JSON_OBJECT 使用 */
    char * key;

    struct json_object_s * next;
} json_object_t;

typedef struct json_task_s {
    char * err_msg;
    unsigned int count;
    unsigned int status;
    char * str;
    unsigned int len;
    void (*callback)( struct json_task_s * task, json_object_t * node );
    json_object_t * root;
} json_task_t;

int json_parser( json_task_t *task );
void json_err_psotion( json_task_t *task, int *line, int *row );

json_object_t * json_append(json_object_t * obj, const char * key, size_t key_len, json_type_t value_type, void * value, size_t value_len);

json_object_t * json_create_object();
json_object_t * json_create_array();

void json_delete_object(json_object_t * obj);

void json_print_value(json_object_t * obj, char **buf, size_t *buf_len);
void json_print(json_object_t * obj, char **buf, size_t *buf_len);

#ifdef	__cplusplus
}
#endif

#endif	/* __LIBJSON_H__ */

