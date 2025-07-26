#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
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
        JSONN_UNEXPECTED,
        JSONN_EOF,
} jsonn_type;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonn_string

typedef union {
        uint64_t long_number;
        double double_number;
} jsonn_number;

typedef struct {
        size_t at;
        char *message;
} jsonn_error;

typedef struct {
        jsonn_result_type type;
        union {
                jsonn_number number;
                jsonn_string string;
                jsonn_error error;
        } is;
} jsonn_result;

typedef struct {
        uint8_t *start;
        uint8_t *current;
        uint8_t *last;
        uint8_t *write;
        jsonn_next next;
        uint16_t flags;
        uint8_t terminator;
        uint8_t quote;
        size_t stack_size;
        size_t stack_pointer;
        uint8_t *stack;
} jsonn_context;

typedef jsonn_context *jsonn_parser;

typedef struct {
        int (*j_literal)(jsonn_parser, jsonn_type);
        int (*j_number)(jsonn_parser, jsonn_type, jsonn_number);
        int (*j_string)(jsonn_parser, jsonn_string);
        int (*j_key)(jsonn_parser, jsonn_string);
        int (*j_begin_array)(jsonn_parser);
        int (*j_end_array)(jsonn_parser);
        int (*j_begin_object)(jsonn_parser);
        int (*j_end_object)(jsonn_parser);
        int (*j_unexpected)(jsonn_parser, jsonn_error);
} jsonn_callbacks;

jsonn_allocator(void *(*malloc)(size_t), void (*free)(void *));

jsonn_parser jsonn_new(uint8_t* config);
void jsonn_free(jsonn_parser p);

jsonn_type jsonn_parse(
                jsonn_parser parser, 
                uint8_t *json, 
                size_t length,
                jsonn_result *result,
                /* nullable */ jsonn_callbacks *callbacks);

jsonn_type jsonn_next(jsonn_parser parser, jsonn_result *result);

/* TODO: _find, _find_any, ... ? */
