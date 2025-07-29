#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef JSONN_STACK_SIZE
#define JSONN_STACK_SIZE 1024
#endif

#define JSONN_FLAG_REPLACE_ILLFORMED_UTF8     0x01
#define JSONN_FLAG_COMMENTS                   0x02
#define JSONN_FLAG_TRAILING_COMMAS            0x04
#define JSONN_FLAG_SINGLE_QUOTES              0x08
#define JSONN_FLAG_UNQUOTED_KEYS              0x10
#define JSONN_FLAG_UNQUOTED_STRINGS           0x20
#define JSONN_FLAG_ESCAPE_CHARACTERS          0x40
#define JSONN_FLAG_OPTIONAL_COMMAS            0x80
#define JSONN_FLAG_IS_OBJECT                  0x100
#define JSONN_FLAG_IS_ARRAY                   0x200

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
        JSONN_EOF,
        JSONN_OPTIONAL,
        JSONN_ERROR
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
        size_t stack_size;
        int16_t flags;
} jsonn_config;

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

jsonn_config jsonn_config_get();
void jsonn_config_set(jsonn_config *config);

jsonn_parser jsonn_new(/* nullable */ jsonn_config *config);
void jsonn_free(jsonn_parser p);

jsonn_type jsonn_parse(
                jsonn_parser parser, 
                uint8_t *json, 
                size_t length,
                /* nullable */ jsonn_callbacks *callbacks);

jsonn_type jsonn_parse_next(jsonn_parser parser);

/* TODO: _find, _find_any, ... ? */
