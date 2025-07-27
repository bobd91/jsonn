#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
        JSONN_BEGIN,
        JSONN_FALSE,
        JSONN_NULL,
        JSONN_TRUE,
        JSONN_LONG,
        JSONN_DOUBLE,
        JSONN_STRING,
        JSONN_KEY,
        JSONN_BEGIN_ARRAY,
        JSONN_END_ARRAY,
        JSONN_BEGIN_OBJECT,
        JSONN_END_OBJECT,
        JSONN_ERROR,
        JSONN_EOF,
} jsonn_type;

typedef enum {
        JSONN_ERROR_NONE = 0,
        JSONN_ERROR_CONFIG,
        JSONN_ERROR_ALLOC,
        JSONN_ERROR_PARSE,
        JSONN_ERROR_ILLFORMED_UTF8,
        JSONN_ERROR_STACKUNDERFLOW,
        JSONN_ERROR_STACKOVERFLOW
} jsonn_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonn_string;

typedef union {
        uint64_t long_value;
        double double_value;
} jsonn_number;

typedef struct {
        jsonn_error_code code;
        size_t at;
} jsonn_error;

typedef struct {
        union {
                jsonn_number number;
                jsonn_string string;
                jsonn_error error;
        } is;
} jsonn_result;

struct jsonn_context;
typedef struct jsonn_context *jsonn_parser;

typedef struct {
        int (*j_boolean)(jsonn_parser, int);
        int (*j_null)(jsonn_parser);
        int (*j_long)(jsonn_parser, int64_t);
        int (*j_double)(jsonn_parser, double);
        int (*j_string)(jsonn_parser, jsonn_string *);
        int (*j_key)(jsonn_parser, jsonn_string *);
        int (*j_begin_array)(jsonn_parser);
        int (*j_end_array)(jsonn_parser);
        int (*j_begin_object)(jsonn_parser);
        int (*j_end_object)(jsonn_parser);
        int (*j_error)(jsonn_parser, jsonn_error *);
} jsonn_callbacks;

void jsonn_allocator(void *(*malloc)(size_t), void (*free)(void *));

jsonn_parser jsonn_new(const char* config, jsonn_error *error);
void jsonn_free(jsonn_parser p);

jsonn_type jsonn_parse(
                jsonn_parser parser, 
                uint8_t *json, 
                size_t length,
                /* nullable */ jsonn_callbacks *callbacks);

jsonn_type jsonn_parse_next(jsonn_parser parser);

jsonn_result *jsonn_parse_result(jsonn_parser parser);

/* TODO: _find, _find_any, ... ? */
