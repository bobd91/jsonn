#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef JSONN_STACK_SIZE
#define JSONN_STACK_SIZE 1024
#endif

#define JSONN_FLAG_COMMENTS                   0x01
#define JSONN_FLAG_TRAILING_COMMAS            0x02
#define JSONN_FLAG_SINGLE_QUOTES              0x04
#define JSONN_FLAG_UNQUOTED_KEYS              0x08
#define JSONN_FLAG_UNQUOTED_STRINGS           0x10
#define JSONN_FLAG_ESCAPE_CHARACTERS          0x20
#define JSONN_FLAG_OPTIONAL_COMMAS            0x40
#define JSONN_FLAG_IS_OBJECT                  0x80
#define JSONN_FLAG_IS_ARRAY                   0x100

typedef enum {
        JSONN_ROOT,
        JSONN_FALSE,
        JSONN_NULL,
        JSONN_TRUE,
        JSONN_INTEGER,
        JSONN_REAL,
        JSONN_STRING,
        JSONN_STRING_NEXT,
        JSONN_KEY,
        JSONN_KEY_NEXT,
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
        JSONN_ERROR_STACKOVERFLOW,
        JSONN_ERROR_FILE_READ
} jsonn_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonn_string_val;

typedef union {
        uint64_t integer;
        double real;
} jsonn_number_val;

typedef struct {
        jsonn_error_code code;
        size_t at;
} jsonn_error_val;

typedef union {
        jsonn_number_val number;
        jsonn_string_val string;
        jsonn_error_val error;
} jsonn_value;

struct jsonn_parser_s;
typedef struct jsonn_parser_s *jsonn_parser;

typedef struct {
        size_t stack_size;
        int16_t flags;
} jsonn_config;

typedef struct jsonn_node_s *jsonn_node;
struct jsonn_node_s {
        jsonn_value is;
        jsonn_type type;
};

typedef struct {
        int (*boolean)(void *ctx, int is_true);
        int (*null)(void *ctx);
        int (*integer)(void *ctx, int64_t integer);
        int (*real)(void *ctx, double real);
        int (*string)(void *ctx, uint8_t *bytes, size_t length);
        int (*key)(void *ctx, uint8_t *bytes , size_t length);
        int (*begin_array)(void *ctx);
        int (*end_array)(void *ctx);
        int (*begin_object)(void *ctx);
        int (*end_object)(void *ctx);
        int (*error)(void *ctx, jsonn_error_code code, int at);
} jsonn_callbacks;

typedef struct jsonn_visitor_s {
        jsonn_callbacks *callbacks;
        void *ctx;
} *jsonn_visitor;

typedef struct jsonn_print_ctx_s *jsonn_print_ctx;

void jsonn_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

jsonn_config jsonn_config_get();
void jsonn_config_set(jsonn_config *config);

jsonn_parser jsonn_new(/* nullable */ jsonn_config *config);
void jsonn_free(jsonn_parser p);

jsonn_type jsonn_parse(
                jsonn_parser parser, 
                uint8_t *json, 
                size_t length,
                jsonn_visitor visitor);

jsonn_type jsonn_parse_fd(
                jsonn_parser parser,
                int fd,
                jsonn_visitor visitor);

jsonn_node jsonn_parse_tree(jsonn_parser p,
                uint8_t *json,
                size_t length);

int jsonn_tree_visit(jsonn_node root, jsonn_visitor visitor);

void jsonn_tree_free(jsonn_node root);

jsonn_visitor jsonn_tree_builder();
jsonn_visitor jsonn_stringifier();
jsonn_visitor jsonn_prettifier();

void jsonn_parse_start(jsonn_parser parser, uint8_t *json, size_t length);
jsonn_type jsonn_parse_next(jsonn_parser parser);

/* TODO: _find, _find_any, ... ? */
