#include <stdlib.h>
#include <string.h>

#include "jsonn.h"

#include "parse.c"

static void *(*jsonn_alloc)(size_t) = malloc;
static void (*jsonn_dealloc)(void *) = free;

static jsonn_type jsonn_parse_callback(
                jsonn_parser p,
                jsonn_callbacks *callbacks,
                void *ctx)
{
        jsonn_type type;
        int abort = 0;
        while(!abort && JSONN_EOF != (type = jsonn_next(p))) {
                switch(type) {
                case JSONN_FALSE:
                case JSONN_TRUE:
                        abort = callbacks->j_boolean 
                                && callbacks->j_boolean(ctx, JSONN_TRUE == type);
                        break;
                case JSONN_NULL:
                        abort = callbacks->j_null 
                                && callbacks->j_null(ctx);
                        break;
                case JSONN_INTEGER:
                        abort = callbacks->j_integer
                                && callbacks->j_integer(ctx, p->result.is.number.integer);
                        break;
                case JSONN_REAL:
                        abort = callbacks->j_real 
                                && callbacks->j_real(ctx, p->result.is.number.real);
                        break;
                case JSONN_STRING:
                        abort = callbacks->j_string
                                && callbacks->j_string(ctx, &p->result.is.string);
                        break;
                case JSONN_KEY:
                        abort = callbacks->j_key
                                && callbacks->j_key(ctx, &p->result.is.string);
                        break;
                case JSONN_BEGIN_ARRAY:
                        abort = callbacks->j_begin_array 
                                && callbacks->j_begin_array(ctx);
                        break;
                case JSONN_END_ARRAY:
                        abort = callbacks->j_end_array 
                                && callbacks->j_end_array(ctx);
                        break;
                case JSONN_BEGIN_OBJECT:
                        abort = callbacks->j_begin_object 
                                && callbacks->j_begin_object(ctx);
                        break;
                case JSONN_END_OBJECT:
                        abort = callbacks->j_end_object 
                                && callbacks->j_end_object(ctx);
                        break;
                case JSONN_ERROR:
                        abort = callbacks->j_error 
                                && callbacks->j_error(ctx, &p->result.is.error);
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

        size_t struct_bytes = sizeof(struct jsonn_parser_s);
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
                jsonn_callbacks *callbacks,
                void *ctx)
{
        p->next = jsonn_init_next(p);
        p->start = p->current = p->write = json;
        p->last = json + length;
        *p->last = '\0';
        p->stack_pointer = 0;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return callbacks
                ? jsonn_parse_callback(p, callbacks, ctx)
                : JSONN_START;
}

void jsonn_parse_start(
                jsonn_parser p,
                uint8_t *json,
                size_t length)
{
        jsonn_parse(p, json, length, NULL, NULL);
}

jsonn_type jsonn_parse_next(jsonn_parser p)
{
        return jsonn_next(p);
}
