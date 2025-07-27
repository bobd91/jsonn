#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jsonn.h"

#include "parse.c"

static void *(*jsonn_alloc)(size_t) = malloc;
static void (*jsonn_dealloc)(void *) = free;

static void jsonn_init(jsonn_parser p, uint8_t *json, size_t length) 
{
        p->next = jsonn_init_next(p);
        p->start = p->current = json;
        p->last = json + length;
        *p->last = '\0';
        p->stack_pointer = 0;
        p->terminator = '\0';

        // Skip leading byte order mark
        p->current += bom_bytes(p);
}

static jsonn_type jsonn_parse_callback(
                jsonn_parser p,
                jsonn_callbacks *callbacks)
{
        jsonn_type type;
        int again = 1;
        while(JSONN_EOF != (type = jsonn_next(p)) && again) {
                switch(type) {
                case JSONN_FALSE:
                case JSONN_TRUE:
                        again = callbacks->j_boolean 
                                && callbacks->j_boolean(p, type);
                case JSONN_NULL:
                        again = callbacks->j_null 
                                && callbacks->j_null(p);

                case JSONN_LONG:
                        again = callbacks->j_long
                                && callbacks->j_long(p, p->result.is.number.long_value);
                        break;

                case JSONN_DOUBLE:
                        again = callbacks->j_double 
                                && callbacks->j_double(p, p->result.is.number.double_value);
                        break;

                case JSONN_STRING:
                        again = callbacks->j_string
                                && callbacks->j_string(p, &p->result.is.string);
                        break;

                case JSONN_KEY:
                        again = callbacks->j_key
                                && callbacks->j_key(p, &p->result.is.string);
                        break;

                case JSONN_BEGIN_ARRAY:
                        again = callbacks->j_begin_array 
                                && callbacks->j_begin_array(p);
                        break;

                case JSONN_END_ARRAY:
                        again = callbacks->j_end_array 
                                && callbacks->j_end_array(p);
                        break;

                case JSONN_BEGIN_OBJECT:
                        again = callbacks->j_begin_object 
                                && callbacks->j_begin_object(p);
                        break;

                case JSONN_END_OBJECT:
                        again = callbacks->j_end_object 
                                && callbacks->j_end_object(p);
                        break;

                case JSONN_ERROR:
                        // If no callback then default to exit parse
                        again = (!callbacks->j_error) 
                                || callbacks->j_error(p, &p->result.is.error);
                        break;

                default:
                        again = 0;
                }
        }

        return type;
}

void jsonn_set_allocator(void *(malloc)(size_t), void (*free)(void *))
{
        jsonn_alloc = malloc;
        jsonn_dealloc = free;
}

jsonn_parser jsonn_new(char *config, jsonn_error *error)
{
        parser_config c = config_parse(config, error);
        if(error->code) {
                return NULL;
        }
        
        size_t struct_bytes = sizeof(struct jsonn_context);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = 1 + (c.stack_size -1) / 8;
        jsonn_parser p = jsonn_alloc(struct_bytes + stack_bytes);
        if(!p) {
                error->code = JSONN_ERROR_ALLOC;
                error->at = 0;
                return NULL;
        }

        p->flags = c.flags;
        p->stack_size = c.stack_size;
        p->stack = (uint8_t *)(p + struct_bytes);

        return p;
}

void jsonn_free(jsonn_parser p) 
{
        jsonn_dealloc(p);
}


jsonn_type jsonn_parse(jsonn_parser p, 
                uint8_t *json, size_t length,
                jsonn_callbacks *callbacks)
{
        jsonn_init(p, json, length);

        return callbacks
                ? jsonn_parse_callback(p, callbacks)
                : jsonn_parse_next(p);
}

jsonn_type jsonn_parse_next(jsonn_parser p)
{
        return jsonn_next(p);
}
