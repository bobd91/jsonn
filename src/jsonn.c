#include <stdlib.h>
#include <string.h>

#include "jsonn.h"

#include "parse.c"

static void *(*jsonn_alloc)(size_t) = malloc;
static void (*jsonn_dealloc)(void *) = free;

static jsonn_type jsonn_parse_callback(
                jsonn_parser p,
                jsonn_callbacks *callbacks)
{
        jsonn_type type;
        int abort = 0;
        while(JSONN_EOF != (type = jsonn_next(p)) && !abort) {
                switch(type) {
                case JSONN_FALSE:
                case JSONN_TRUE:
                        abort = callbacks->j_boolean 
                                && callbacks->j_boolean(p, JSONN_TRUE == type);
                        break;
                case JSONN_NULL:
                        abort = callbacks->j_null 
                                && callbacks->j_null(p);
                        break;
                case JSONN_LONG:
                        abort = callbacks->j_long
                                && callbacks->j_long(p, p->result.is.number.long_value);
                        break;
                case JSONN_DOUBLE:
                        abort = callbacks->j_double 
                                && callbacks->j_double(p, p->result.is.number.double_value);
                        break;
                case JSONN_STRING:
                        abort = callbacks->j_string
                                && callbacks->j_string(p, &p->result.is.string);
                        break;
                case JSONN_KEY:
                        abort = callbacks->j_key
                                && callbacks->j_key(p, &p->result.is.string);
                        break;
                case JSONN_BEGIN_ARRAY:
                        abort = callbacks->j_begin_array 
                                && callbacks->j_begin_array(p);
                        break;
                case JSONN_END_ARRAY:
                        abort = callbacks->j_end_array 
                                && callbacks->j_end_array(p);
                        break;
                case JSONN_BEGIN_OBJECT:
                        abort = callbacks->j_begin_object 
                                && callbacks->j_begin_object(p);
                        break;
                case JSONN_END_OBJECT:
                        abort = callbacks->j_end_object 
                                && callbacks->j_end_object(p);
                        break;
                case JSONN_ERROR:
                        abort = callbacks->j_error 
                                && callbacks->j_error(p, &p->result.is.error);
                        abort = 1; // always abort after error
                        break;
                default:
                        abort = 1;
                }
        }

        return type;
}

void jsonn_set_allocator(void *(malloc)(size_t), void (*free)(void *))
{
        jsonn_alloc = malloc;
        jsonn_dealloc = free;
}

jsonn_parser jsonn_new(jsonn_config *config)
{
        jsonn_config c = config_select(config); 

        size_t struct_bytes = sizeof(struct jsonn_context);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (c.stack_size + 7) / 8;
        jsonn_parser p = jsonn_alloc(struct_bytes + stack_bytes);
        if(p) {
                p->stack_size = c.stack_size;
                p->stack = (uint8_t *)(p + struct_bytes);
                p->flags = c.flags;
        }
        return p;
}

void jsonn_free(jsonn_parser p) 
{
        if(p) jsonn_dealloc(p);
}

jsonn_type jsonn_parse(
                jsonn_parser p, 
                uint8_t *json, 
                size_t length,
                jsonn_callbacks *callbacks)
{
        p->next = jsonn_init_next(p);
        p->start = p->current = p->write = json;
        p->last = json + length;
        *p->last = '\0';
        p->stack_pointer = 0;
        p->terminator = '\0';

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return callbacks
                ? jsonn_parse_callback(p, callbacks)
                : JSONN_BEGIN;
}

jsonn_type jsonn_parse_next(jsonn_parser p)
{
        return jsonn_next(p);
}
