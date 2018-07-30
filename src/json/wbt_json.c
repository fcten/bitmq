#include "wbt_json.h"
#include "../webit.h"
#include "../common/wbt_memory.h"

enum {
    STS_START = 0,
    STS_END,
    STS_OBJECT_START,
    STS_OBJECT_COLON,
    STS_OBJECT_VALUE,
    STS_OBJECT_COMMA,
    STS_ARRAY_START,
    STS_ARRAY_COMMA,
    STS_STRING_START,
    STS_STRING_ESCAPE,
    STS_STRING_HEX1,
    STS_STRING_HEX2,
    STS_STRING_HEX3,
    STS_STRING_HEX4,
    STS_NUMBER_START,
    STS_NUMBER_POSITIVE,
    STS_NUMBER_A,
    STS_NUMBER_B,
    STS_NUMBER_DECIMAL,
    STS_NUMBER_EXPONENT,
    STS_NUMBER_EXPONENT_A,
    STS_NUMBER_EXPONENT_DIGIT,
    STS_NUMBER_EXPONENT_DIGIT_A
};

int json_parse_true( json_task_t *task );
int json_parse_false( json_task_t *task );
int json_parse_null( json_task_t *task );
int json_parse_number( json_task_t *task, json_object_t *parent );
int json_parse_string( json_task_t *task );
int json_parse_value( json_task_t *task, json_object_t *parent );
int json_parse_array( json_task_t *task, json_object_t *parent );
int json_parse_object( json_task_t *task, json_object_t *parent );

int json_parse_true( json_task_t *task ) {
    if( task->count + 3 < task->len &&
        *(task->str + task->count) == 'r' &&
        *(task->str + task->count + 1) == 'u' &&
        *(task->str + task->count + 2) == 'e' ) {
        task->count += 3;
        return 0;
    }
    
    task->err_msg = "expect 'true'";
    return -1;
}

int json_parse_false( json_task_t *task ) {
    if( task->count + 4 < task->len &&
        *(task->str + task->count) == 'a' &&
        *(task->str + task->count + 1) == 'l' &&
        *(task->str + task->count + 2) == 's' &&
        *(task->str + task->count + 3) == 'e' ) {
        task->count += 4;
        return 0;
    }
    
    task->err_msg = "expect 'false'";
    return -1;
}

int json_parse_null( json_task_t *task ) {
    if( task->count + 3 < task->len &&
        *(task->str + task->count) == 'u' &&
        *(task->str + task->count + 1) == 'l' &&
        *(task->str + task->count + 2) == 'l' ) {
        task->count += 3;
        return 0;
    }
    
    task->err_msg = "expect 'null'";
    return -1;
}

int json_parse_number( json_task_t *task, json_object_t *parent ) {
    char ch;
    task->status = STS_NUMBER_START;
    while(( ch = *(task->str + task->count) )) {
        task->count ++;

        switch( task->status ) {
            case STS_NUMBER_START:
                if( ch == '-' ) {
                    task->status = STS_NUMBER_POSITIVE;
                } else {
                    task->count --;
                    task->status = STS_NUMBER_POSITIVE;
                }
                break;
            case STS_NUMBER_POSITIVE:
                if( ch == '0' ) {
                    task->status = STS_NUMBER_A;
                } else if ( ch >= '1' && ch <= '9' ) {
                    task->status = STS_NUMBER_B;
                } else {
                    task->err_msg = "expect '0-9'";
                    return -1;
                }
                break;
            case STS_NUMBER_B:
                if ( ch >= '0' && ch <= '9' ) {
                    // do nothing
                } else {
                    task->count --;
                    task->status = STS_NUMBER_A;
                }
                break;
            case STS_NUMBER_A:
                if( ch == '.' ) {
                    parent->value_type = JSON_DOUBLE;
                    task->status = STS_NUMBER_DECIMAL;
                } else {
                    task->count --;
                    task->status = STS_NUMBER_EXPONENT;
                }
                break;
            case STS_NUMBER_DECIMAL:
                if ( ch >= '0' && ch <= '9' ) {
                    // do nothing
                } else {
                    task->count --;
                    task->status = STS_NUMBER_EXPONENT;
                }
                break;
            case STS_NUMBER_EXPONENT:
                if( ch == 'e' || ch == 'E' ) {
                    task->status = STS_NUMBER_EXPONENT_A;
                } else {
                    task->count --;
                    return 0;
                }
                break;
            case STS_NUMBER_EXPONENT_A:
                if( ch == '+' || ch == '-' ) {
                    task->status = STS_NUMBER_EXPONENT_DIGIT;
                } else {
                    task->count --;
                    task->status = STS_NUMBER_EXPONENT_DIGIT;
                }
                break;
            case STS_NUMBER_EXPONENT_DIGIT:
                if ( ch >= '0' && ch <= '9' ) {
                    task->status = STS_NUMBER_EXPONENT_DIGIT_A;
                } else {
                    task->err_msg = "expect '0-9'";
                    return -1;
                }
                break;
            case STS_NUMBER_EXPONENT_DIGIT_A:
                if ( ch >= '0' && ch <= '9' ) {
                    task->status = STS_NUMBER_EXPONENT_DIGIT_A;
                } else {
                    task->count --;
                    return 0;
                }
                break;
            default:
                task->err_msg = "unknown status";
                return -1;
        }
    }
    
    task->err_msg = "unexpect EOF";
    return -1;
}

int json_parse_string( json_task_t *task ) {
    char ch;
    task->status = STS_STRING_START;
    while(( ch = *(task->str + task->count) )) {
        task->count ++;

        switch( task->status ) {
            case STS_STRING_START:
                if( ch == '"' ) {
                    return 0;
                } else if( ch == '\\' ) {
                    task->status = STS_STRING_ESCAPE;
                } else {
                    // do nothing
                }
                break;
            case STS_STRING_ESCAPE:
                if( ch == '"' || ch == '\\' || ch == '/' ||  ch == 'b' ||
                    ch == 'f' || ch == 'n'  || ch == 'r' || ch == 't' ) {
                    task->status = STS_STRING_START;
                } else if( ch == 'u' ) {
                    task->status = STS_STRING_HEX1;
                } else {
                    task->err_msg = "illegal escape";
                    return -1;
                }
                break;
            case STS_STRING_HEX1:
            case STS_STRING_HEX2:
            case STS_STRING_HEX3:
            case STS_STRING_HEX4:
                if( ( ch >= '0' && ch <= '9' ) ||
                    ( ch >= 'a' && ch <= 'f' ) ||
                    ( ch >= 'A' && ch <= 'F' ) ) {
                    if( task->status == STS_STRING_HEX4 ) {
                        task->status = STS_STRING_START;
                    } else {
                        task->status ++;
                    }
                } else {
                    task->err_msg = "expect '0-9', 'a-f', 'A-F'";
                    return -1;
                }
                break;
            default:
                task->err_msg = "unknown status";
                return -1;
        }
    }
    
    task->err_msg = "unexpect EOF";
    return -1;
}

int json_parse_value( json_task_t *task, json_object_t *parent ) {
    char ch;
    while(( ch = *(task->str + task->count) )) {
        task->count ++;

        if( ch == '"' ) {
            parent->value_type = JSON_STRING;
            return json_parse_string( task );
        } else if( ch == '-' || ( ch >= '0' && ch <= '9' ) ) {
            task->count --;
            parent->value_type = JSON_LONGLONG;
            return json_parse_number( task, parent );
        } else if( ch == '{' ) {
            parent->value_type = JSON_OBJECT;
            return json_parse_object( task, parent );
        } else if( ch == '[' ) {
            parent->value_type = JSON_ARRAY;
            return json_parse_array( task, parent );
        } else if( ch == 't' ) {
            parent->value_type = JSON_TRUE;
            return json_parse_true( task );
        } else if( ch == 'f' ) {
            parent->value_type = JSON_FALSE;
            return json_parse_false( task );
        } else if( ch == 'n' ) {
            parent->value_type = JSON_NULL;
            return json_parse_null( task );
        } else {
            task->err_msg = "illegal value";
            return -1;
        }
    }

    task->err_msg = "unexpect EOF";
    return -1;
}

int json_parse_array( json_task_t *task, json_object_t *parent ) {
    char ch;
    json_object_t node, * append = NULL;
    
    node.next = parent;
    node.key = NULL;
    node.key_len = 0;
    
    if( !task->callback ) {
        if( !task->root ) {
            append = task->root = json_create_array();
        } else {
            append = parent->value.p = json_create_array();
        }
    }
    
    task->status = STS_ARRAY_START;

    while(( ch = *(task->str + task->count) )) {
        task->count ++;
        
        if( ch == ' ' || ch == '\n' || ch == '\t' ) {
            continue;
        }
        
        switch( task->status ) {
            case STS_ARRAY_START:
                if( ch == ']' ) {
                    return 0;
                } else {
                    /* 这里需要将计数减一，因为 json_parse_value 需要根据这一个字符判断 value 类型 */
                    task->count --;

                    node.value.s = task->str + task->count;
                    if( json_parse_value( task, &node ) != 0 ) {
                        if( node.value_type == JSON_OBJECT ||
                            node.value_type == JSON_ARRAY ) {
                            json_delete_object(node.value.p);
                        }
                        return -1;
                    }
                    // WARNING: value_len 可能发生溢出
                    node.value_len = task->str + task->count - node.value.s;
                    
                    // 对 value 进行处理
                    switch(node.value_type) {
                        case JSON_STRING:
                            // 去除字符串两端的引号
                            node.value.s += 1;
                            node.value_len -= 2;
                            break;
                        case JSON_DOUBLE:
                            // TODO 在解析测试用例时，atof 与 atoll 使解析时间延长了几乎一半
                            // 移除对数值的默认解析可以显著提升性能
                            node.value.d = atof(node.value.s);
                            break;
                        case JSON_LONGLONG:
                            node.value.l = atoll(node.value.s);
                            break;
                        case JSON_ARRAY:
                        case JSON_OBJECT:
                            break;
                    }

                    // 需要放在 node.key_len ++ 之前以便正确输出数组下标
                    if( task->callback ) {
                        task->callback( task, &node );
                    }

                    if( !task->callback ) {
                        switch( node.value_type ) {
                            case JSON_ARRAY:
                            case JSON_OBJECT:
                            case JSON_STRING:
                                append = json_append(append, node.key, node.key_len,
                                        node.value_type, node.value.p, node.value_len);
                                break;
                            default:
                                append = json_append(append, node.key, node.key_len,
                                        node.value_type, &node.value, node.value_len);
                        }
                    }
                    
                    node.key_len ++;
                    task->status = STS_ARRAY_COMMA;
                }
                break;
            case STS_ARRAY_COMMA:
                if( ch == ',' ) {
                    task->status = STS_ARRAY_START;
                } else if( ch == ']' ) {
                    return 0;
                } else {
                    task->err_msg = "expect ',' or ']'";
                    return -1;
                }
                break;
            default:
                task->err_msg = "unknown status";
                return -1;
        }
    }

    task->err_msg = "unexpect EOF";
    return -1;
}

int json_parse_object( json_task_t *task, json_object_t *parent ) {
    char ch;
    json_object_t node, * append = NULL;
    
    node.next = parent;
    node.key = NULL;
    node.key_len = 0;
    
    if( !task->callback ) {
        if( !task->root ) {
            append = task->root = json_create_object();
        } else {
            append = parent->value.p = json_create_object();
        }
    }
    
    task->status = STS_OBJECT_START;

    while(( ch = *(task->str + task->count) )) {
        task->count ++;
        
        if( ch == ' ' || ch == '\n' || ch == '\t' ) {
            continue;
        }
        
        switch( task->status ) {
            case STS_OBJECT_START:
                if( ch == '"' ) {
                    node.key = task->str + task->count;
                    if( json_parse_string( task ) != 0 ) {
                        return -1;
                    }
                    // WARNING: key_len 可能发生溢出
                    node.key_len = task->str + task->count - node.key - 1;
                    
                    task->status = STS_OBJECT_COLON;
                } else if( ch == '}' ) {
                    // 空对象 {}，忽略
                    return 0;
                } else {
                    task->err_msg = "expect '\"' or '}'";
                    return -1;
                }
                break;
            case STS_OBJECT_COLON:
                if( ch == ':' ) {
                    task->status = STS_OBJECT_VALUE;
                } else {
                    task->err_msg = "expect ':'";
                    return -1;
                }
                break;
            case STS_OBJECT_VALUE:
                /* 这里需要将计数减一，因为 json_parse_value 需要根据这一个字符判断 value 类型 */
                task->count --;
                
                node.value.s = task->str + task->count;
                if( json_parse_value( task, &node ) != 0 ) {
                    if( node.value_type == JSON_OBJECT ||
                        node.value_type == JSON_ARRAY ) {
                        json_delete_object(node.value.p);
                    }
                    return -1;
                }
                // WARNING: value_len 可能发生溢出
                node.value_len = task->str + task->count - node.value.s;

                task->status = STS_OBJECT_COMMA;
                break;
            case STS_OBJECT_COMMA:
                if( ( ch == ',' || ch == '}' ) ) {
                    // 对 value 进行处理
                    switch(node.value_type) {
                        case JSON_STRING:
                            // 去除字符串两端的引号
                            node.value.s += 1;
                            node.value_len -= 2;
                            break;
                        case JSON_DOUBLE:
                            node.value.d = atof(node.value.s);
                            break;
                        case JSON_LONGLONG:
                            node.value.l = atoll(node.value.s);
                            break;
                        case JSON_ARRAY:
                        case JSON_OBJECT:
                            break;
                    }

                    if( task->callback ) {
                        task->callback( task, &node );
                    }
                    
                    if( !task->callback ) {
                        switch( node.value_type ) {
                            case JSON_ARRAY:
                            case JSON_OBJECT:
                            case JSON_STRING:
                                append = json_append(append, node.key, node.key_len,
                                        node.value_type, node.value.p, node.value_len);
                                break;
                            default:
                                append = json_append(append, node.key, node.key_len,
                                        node.value_type, &node.value, node.value_len);
                        }
                    }
                }
                
                if( ch == ',' ) {
                    task->status = STS_OBJECT_START;
                } else if( ch == '}' ) {
                    return 0;
                } else {
                    task->err_msg = "expect ',' or '}'";
                    return -1;
                }
                break;
            default:
                task->err_msg = "unknown status";
                return -1;
        }
    }
    
    task->err_msg = "unexpect EOF";
    return -1;
}

void json_err_psotion( json_task_t *task, int *line, int *row ) {
    int p = 0;
    char ch;
    (*line) = 1;
    (*row) = 0;
    while(( ch = *(task->str + p) )) {
        p ++;
        
        if( ch == '\n' ) {
            (*line) ++;
            (*row) = 0;
        } else {
            (*row) ++;
        }
        
        if( p == task->count ) break;
    }
}

int json_parser( json_task_t *task ) {
    char ch;

    task->err_msg = "OK";
    task->count = 0;
    task->status = STS_START;
    task->root = NULL;
    
    if( task->str == NULL || task->len <= 0 ) {
        task->err_msg = "the input string is not specified";
        return -1;
    }
    
    while( task->count < task->len ) {
        ch = *(task->str + task->count);
        task->count ++;
        
        if( ch == ' ' || ch == '\n' || ch == '\t' ) {
            continue;
        }

        switch( task->status ) {
            case STS_START:
                if( ch == '{' ) {
                    if( json_parse_object( task, task->root ) != 0 ) {
                        return -1;
                    }
                    task->status = STS_END;
                } else if( ch == '[' ) {
                    if( json_parse_array( task, task->root ) != 0 ) {
                        return -1;
                    }
                    task->status = STS_END;
                } else {
                    task->err_msg = "expect '{' or '['";
                    return -1;
                }
                break;
            case STS_END:
                if( task->count >= task->len ) {
                    return 0;
                } else {
                    task->err_msg = "expect EOF";
                    return -1;
                }
                break;
            default:
                task->err_msg = "unknown status";
                return -1;
        }
    }
    
    if( task->status == STS_END ) {
        return 0;
    } else {
        return -1;
    }
}

json_object_t * json_create_object() {
    json_object_t * obj = (json_object_t *) wbt_malloc(sizeof(json_object_t));
    if( obj ) {
        memset(obj, 0, sizeof(json_object_t));
        obj->object_type = JSON_OBJECT;
    }
    
    return obj;
}

json_object_t * json_create_array() {
    json_object_t * obj = (json_object_t *) wbt_malloc(sizeof(json_object_t));
    if( obj ) {
        memset(obj, 0, sizeof(json_object_t));
        obj->object_type = JSON_ARRAY;
    }
    
    return obj;
}

// 如果添加失败 返回 NULL
// 如果添加成功，返回所添加的 json_object_t 结构指针
json_object_t * json_append(json_object_t * obj, const char * key, size_t key_len, json_type_t value_type, void * value, size_t value_len) {
    assert(obj);
    assert( (obj->object_type == JSON_OBJECT && key) || (obj->object_type != JSON_OBJECT) );

    while( obj->value_type != JSON_NONE ) {
        if( !obj->next ) {
            if( obj->object_type == JSON_OBJECT ) {
                obj->next = json_create_object();
            } else if ( obj->object_type == JSON_ARRAY ) {
                obj->next = json_create_array();
            } else {
                return NULL;
            }
        }
        if( obj->next ) {
            obj = obj->next;
        } else {
            return NULL;
        }
    }
    
    if( obj->object_type == JSON_OBJECT ) {
#ifdef __x86_64__
        assert( !(key_len >> 18) );
#elif __i386__
        assert( !(key_len >> 8) );
#endif
        obj->key = (char *)key;
        obj->key_len = key_len;
    }
    
    obj->value_type = value_type;

    switch(value_type) {
        case JSON_OBJECT:
        case JSON_ARRAY:
            obj->value.p = (json_object_t *)value;
            break;
        case JSON_STRING:
#ifdef __x86_64__
            assert( !(value_len >> 38) );
#elif __i386__
            assert( !(value_len >> 16) );
#endif
            obj->value.s = value;
            obj->value_len = value_len;
            break;
        case JSON_DOUBLE:
            obj->value.d = *((double *)value);
            break;
        case JSON_INTEGER:
            obj->value.i = *((int *)value);
            break;
        case JSON_FLOAT:
            obj->value.f = *((float *)value);
            break;
        case JSON_LONGLONG:
            obj->value.l = *((long long *)value);
            break;
        default:
            obj->value.l = 1;
            break;
    }
    
    return obj;
}

void json_print_longlong(long long l, char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "%lld", l);
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_float(float f, char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "%g", f);
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_integer(int i, char **buf, size_t *buf_len) {
    size_t si = snprintf(*buf, *buf_len, "%d", i);
    if( si > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += si;
        *buf_len -= si;
    }
}

void json_print_double(double d, char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "%lg", d);
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_char(char c, char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "%c", c);
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_string(const char * str, size_t len, char **buf, size_t *buf_len) {
    json_print_char('\"', buf, buf_len);
	size_t i;
    for(i = 0 ; i < len ; i++) {
        switch(str[i]) {
            case '\\':
                if( i < len - 1) {
                    json_print_char('\\', buf, buf_len);
                    json_print_char(str[++i], buf, buf_len);
                } else {
                    json_print_char('\\', buf, buf_len);
                    json_print_char('\\', buf, buf_len);
                }
                break;
            case '\"':
                json_print_char('\\', buf, buf_len);
                json_print_char('\"', buf, buf_len);
                break;
            case '\b':
                json_print_char('\\', buf, buf_len);
                json_print_char('b', buf, buf_len);
                break;
            case '\f':
                json_print_char('\\', buf, buf_len);
                json_print_char('f', buf, buf_len);
                break;
            case '\n':
                json_print_char('\\', buf, buf_len);
                json_print_char('n', buf, buf_len);
                break;
            case '\r':
                json_print_char('\\', buf, buf_len);
                json_print_char('r', buf, buf_len);
                break;
            case '\t':
                json_print_char('\\', buf, buf_len);
                json_print_char('t', buf, buf_len);
                break;
            default:
                json_print_char(str[i], buf, buf_len);
        }
    }
    json_print_char('\"', buf, buf_len);
}

void json_print_true(char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "true");
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_false(char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "false");
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_null(char **buf, size_t *buf_len) {
    size_t i = snprintf(*buf, *buf_len, "null");
    if( i > *buf_len ) {
        *buf += *buf_len;
        *buf_len = 0;
    } else {
        *buf += i;
        *buf_len -= i;
    }
}

void json_print_object(json_object_t * obj, char **buf, size_t *buf_len) {
    json_print_char('{', buf, buf_len);
    if( obj->key_len ) {
        while(obj) {
            json_print_string(obj->key, obj->key_len, buf, buf_len);
            json_print_char(':', buf, buf_len);
            json_print_value(obj, buf, buf_len);
        
            obj = obj->next;
        
            if(obj) {
                json_print_char(',', buf, buf_len);
            }
        }
    }
    json_print_char('}', buf, buf_len);
}

void json_print_array(json_object_t * obj, char **buf, size_t *buf_len) {
    json_print_char('[', buf, buf_len);
    while(obj) {
        json_print_value(obj, buf, buf_len);
        
        obj = obj->next;
        
        if(obj) {
            json_print_char(',', buf, buf_len);
        }
    }
    json_print_char(']', buf, buf_len);
}

void json_print_value(json_object_t * obj, char **buf, size_t *buf_len) {
    switch(obj->value_type) {
        case JSON_OBJECT:
            json_print_object(obj->value.p, buf, buf_len);
            break;
        case JSON_ARRAY:
            json_print_array(obj->value.p, buf, buf_len);
            break;
        case JSON_STRING:
            json_print_string(obj->value.s, obj->value_len, buf, buf_len);
            break;
        case JSON_DOUBLE:
            json_print_double(obj->value.d, buf, buf_len);
            break;
        case JSON_INTEGER:
            json_print_integer(obj->value.i, buf, buf_len);
            break;
        case JSON_FLOAT:
            json_print_float(obj->value.f, buf, buf_len);
            break;
        case JSON_LONGLONG:
            json_print_longlong(obj->value.l, buf, buf_len);
            break;
        case JSON_TRUE:
            json_print_true(buf, buf_len);
            break;
        case JSON_FALSE:
            json_print_false(buf, buf_len);
            break;
        case JSON_NULL:
            json_print_null(buf, buf_len);
            break;
        default:
            break;
    }
}

void json_print(json_object_t * obj, char **buf, size_t *buf_len) {
    switch(obj->object_type) {
        case JSON_OBJECT:
            json_print_object(obj, buf, buf_len);
            break;
        case JSON_ARRAY:
            json_print_array(obj, buf, buf_len);
            break;
        default:
            // JSON 对象必须以对象或者数组为顶层元素
            break;
    }
}

void json_delete_object(json_object_t * obj) {
    while(obj) {
        if( obj->value_type == JSON_ARRAY || obj->value_type == JSON_OBJECT ) {
            json_delete_object(obj->value.p);
        }

        json_object_t * next = obj->next;
        wbt_free(obj);
        obj = next;
    }
}