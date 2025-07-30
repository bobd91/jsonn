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
        JSONN_START,
        JSONN_FALSE,
        JSONN_NULL,
        JSONN_TRUE,
        JSONN_INTEGER,
        JSONN_REAL,
        JSONN_STRING,
        JSONN_KEY,
        JSONN_BEGIN_ARRAY,
        JSONN_END_ARRAY,
        JSONN_BEGIN_OBJECT,
        JSONN_END_OBJECT,
        JSONN_OPTIONAL,
        JSONN_ERROR,
        JSONN_EOF
} jsonn_type;

typedef enum {
        JSONN_ERROR_NONE = 0,
        JSONN_ERROR_CONFIG,
        JSONN_ERROR_ALLOC,
        JSONN_ERROR_PARSE,
        JSONN_ERROR_NUMBER,
        JSONN_ERROR_UTF8,
        JSONN_ERROR_STACKUNDERFLOW,
        JSONN_ERROR_STACKOVERFLOW
} jsonn_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonn_string;

typedef union {
        uint64_t integer;
        double real;
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

struct jsonn_parser_s;
typedef struct jsonn_parser_s *jsonn_parser;

typedef struct {
        size_t stack_size;
        int16_t flags;
} jsonn_config;

typedef struct {
        int (*j_boolean)(void *, int);
        int (*j_null)(void *);
        int (*j_integer)(void *, int64_t);
        int (*j_real)(void *, double);
        int (*j_string)(void *, jsonn_string *);
        int (*j_key)(void *, jsonn_string *);
        int (*j_begin_array)(void *);
        int (*j_end_array)(void *);
        int (*j_begin_object)(void *);
        int (*j_end_object)(void *);
        int (*j_error)(void *, jsonn_error *);
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
                jsonn_callbacks *callbacks,
                void *ctx);

void jsonn_parse_start(jsonn_parser parser, uint8_t *json, size_t length);
jsonn_type jsonn_parse_next(jsonn_parser parser);

/* TODO: _find, _find_any, ... ? */
