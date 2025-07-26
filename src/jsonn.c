#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jsonn.h"

void *(*jsonn_alloc)(size_t) = malloc;
void (*jsonn_dealloc)(void *) = free;

void jsonn_set_allocator(void *(malloc)(size_t), void (*free)(void *))
{
        jsonn_alloc = malloc;
        jsonn_dealloc = free;
}

jsonn_parser jsonn_new(uint8_t *config)
{
        jsonn_config c = jsonn_config_parse(config);
        if(!c)
                return NULL;

        size_t struct_bytes = sizeof(jsonn_context);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = 1 + (c.stack_size -1) / 8;
        p = jsonn_alloc(struct_bytes + stack_bytes);
        if(!p)
                return NULL;

        p->flags = c.flags;
        p->stack_size = c.stack_size;
        p->stack = p + struct_bytes;

        return p;
}

void jsonn_free(jsonn_parser p) 
{
        jsonn_dealloc(p);
}

static void json_init(jsonn_parser p, uint8_t *json, size_t length) 
{
        p->next = jsonn_init_next(p);
        p->start = p->current = json;
        p->last = json + length;
        *p->last = NULL;
        p->stack_pointer = 0;
        p->terminator = NULL;
        p->quote = NULL;

        consume_bom();
}

jsonn_type jsonn_parse(jsonn_parser p, 
                uint8_t *json, size_t length,
                jsonn_result *result,
                jsonn_callbacks *callbacks)
{
        jsonn_init(p, json, length);

        return callbacks
                ? jsson_parse_callback(p, result, callbacks)
                : jsson_parse_next(p, result);
}

jsonn_type jsson_next(jsonn_parser p, jsonn_result *result)
{
        return jsonn_parse_next(p. result);
}

static jsonn_type jsson_callback(
                jsonn_parser p,
                jsonn_result *result, 
                jsonn_callbacks *callbacks)
{
        jsonn_type type;
        int res;
        while(JSONN_EOF != (type = jsson_parse_next(p, result))) {
                switch(type) {
                case JSONN_FALSE:
                case JSONN_TRUE:
                case JSONN_NULL:
                        res = callbacks->j_literal 
                                && callbacks->j_literal(p, type);

                case JSONN_LONG:
                        res = callbacks->j_number 
                                && callbacks->j_number(p, type, result);
                        break;

                case JSONN_DOUBLE:
                        res = callbacks->j_number 
                                && callbacks->j_number(p, type, result);
                        break;

                case JSONN_STRING:
                        res = callbacks->j_string
                                && callbacks->j_string(p, result->is.string);
                        break;

                case JSONN_KEY:
                        res = callbacks->j_key
                                && callbacks->j_key(p, result->is.string);
                        break;

                case JSONN_BEGIN_ARRAY:
                        res = callbacks->j_begin_array 
                                && callbacks->j_begin_array(p);
                        break;

                case JSONN_END_ARRAY:
                        res = callbacks->j_end_array 
                                && callbacks->j_end_array(p);
                        break;

                case JSONN_BEGIN_OBJECT:
                        res = callbacks->j_begin_object 
                                && callbacks->j_begin_object(p);
                        break;

                case JSONN_END_OBJECT:
                        res = callbacks->j_end_object 
                                && callbacks->j_end_object(p);
                        break;

                case JSONN_UNEXPECTED:
                        // If no callback then default to unexpected
                        res = (!callbacks->j_unexpected) 
                                || callbacks->j_unexpected(p, result->is.error);
                        break;

                default:
                        res = 0;

                }
                if(!res)
                        return JSONN_UNEXPECTED;
        }

        return JSONN_EOF;
}
