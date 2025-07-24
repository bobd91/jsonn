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
  JSONN_NAME,
  JSONN_BEGIN_ARRAY,
  JSONN_END_ARRAY,
  JSONN_BEGIN_OBJECT,
  JSONN_END_OBJECT,
  JSONN_UNEXPECTED,
  JSONN_EOF,
} jsonn_type;

typedef enum {
  JSONN_LONG_RESULT,
  JSONN_DOUBLE_RESULT,
  JSONN_STRING_RESULT,
  JSONN_ERROR_RESULT
} jsonn_result_type;

typedef struct {
  uint8_t *bytes;
  size_t length;
  int partial;
} jsonn_string

typedef struct {
  json_result_type type;
  union {
    int64_t long_number;
    double double_number;
    jsonn_string string;
    jsonn_error error;
  } is;
} jsonn_result;

typedef struct {
  size_t at;
  char *message;
} jsonn_error;

typedef struct {
  int type;
  jsonn_next next;
  uint8_t flags;
  size_t stack_max;
  size_t stack_pointer;
  uint8_t *stack;
  uint8_t *start;
  uint8_t *current;
  uint8_t *last;
  uint8_t *write;
  FILE *stream;
} jsonn_context;

typedef jsonn_context *jsonn_parser;

typedef struct {
  int (*j_literal)(jsonn_parser, jsonn_type);
  int (*j_long)(jsonn_parser, int64_t);
  int (*j_double)(jsonn_parser, double);
  int (*j_string)(jsonn_parser, jsonn_string);
  int (*j_begin_array)(jsonn_parser);
  int (*j_end_array)(jsonn_parser);
  int (*j_begin_object)(jsonn_parser);
  int (*j_end_object)(jsonn_parser);
  int (*j_unexpected)(jsonn_parser, jsonn_error);
} jsonn_callbacks;

jsonn_set_allocator(void *(*)(size_t));

jsonn_parser jsonn_buffer_parser(uint8_t* config);
jsonn_parser jsonn_stream_parser(uint8_t* config);

void jsonn_init_buffer(jsonn_parser parser, uint8_t *buffer, size_t length);
void jsonn_init_stream(jsonn_parser parser, FILE *stream);

jsonn_type jsonn_next(jsonn_parser parser, jsonn_result *result);
jsonn_type jsonn_callback(jsonn_parser, jsonn_callbacks callbacks, jsonn_result *result);

