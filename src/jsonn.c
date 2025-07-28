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
        p->start = p->current = p->write = json;
        p->last = json + length;
        if(*p->last)
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
                        abort =callbacks->j_error 
                                && callbacks->j_error(p, &p->result.is.error);
                        abort |= 1; // always abort after error
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

static jsonn_parser jsonn_alloc_parser(size_t stack_size, char *extra)
{
        size_t extra_bytes = 0;
        if(extra && *extra) {
                // allow for terminator
                extra_bytes = 1 + strlen(extra);
        }
        size_t struct_bytes = sizeof(struct jsonn_context);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (stack_size + 7) / 8;
        jsonn_parser p = jsonn_alloc(struct_bytes + stack_bytes + extra_bytes);
        if(p) {
                p->stack_size = stack_size;
                p->stack = (uint8_t *)(p + struct_bytes);
                if(extra_bytes) {
                        p->extra = (uint8_t *)(p + struct_bytes + stack_bytes);
                        memcpy(p->extra, extra, extra_bytes);
                } else {
                        p->extra = NULL;
                }
        }
        return p;
}

// Use jsonn_parser to parse config
// config_text will usually be a literal string and so be read-only
// parsing will write into text so we have to take a copy 
static int jsonn_config_parse(
                const char *config_text, 
                parser_config *config, 
                jsonn_error *error) 
{
        // config_text  - the text passed in to create a jsonn parser
        // config       - the data generated from parsing config_text

        // cc           - config for the parser that is parsing config_text

        // Load default config for jsonn parser
        config_copy_defaults(config);

        if(!config_text || !*config_text)
                // no config, default parser
                return 1;

        const parser_config *cc = config_config();
        jsonn_parser config_text_parser =
                jsonn_alloc_parser(cc->stack_size, (char *)config_text);

        if(!config_text_parser) {
                        error->code = JSONN_ERROR_ALLOC;
                        error->at = 0;
                        return 0;
        }

        config_text_parser->flags = cc->flags;
        jsonn_type res = config_parse(config_text_parser, config);
        if(JSONN_ERROR == res) {
                error->code = JSONN_ERROR_CONFIG;
                error->at = config_text_parser->result.is.error.at;
        }
        jsonn_free(config_text_parser);

        return JSONN_EOF == res;
}

jsonn_parser jsonn_new(const char *config, jsonn_error *error)
{
        parser_config pc;
        if(!jsonn_config_parse(config, &pc, error))
                return NULL;
        
        jsonn_parser p = jsonn_alloc_parser(pc.stack_size, NULL);
        if(!p) {
                error->code = JSONN_ERROR_ALLOC;
                error->at = 0;
        } else {
                p->flags = pc.flags;
        }

        return p;
}

void jsonn_free(jsonn_parser p) 
{
        if(p)
                jsonn_dealloc(p);
}

jsonn_type jsonn_parse(jsonn_parser p, 
                uint8_t *json, size_t length,
                jsonn_callbacks *callbacks)
{
        jsonn_init(p, json, length);

        return callbacks
                ? jsonn_parse_callback(p, callbacks)
                : JSONN_BEGIN;
}

jsonn_type jsonn_parse_next(jsonn_parser p)
{
        return jsonn_next(p);
}
