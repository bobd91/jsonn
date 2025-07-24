#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
  JSONN_FALSE,
  JSONN_NULL,
  JSONN_TRUE,
  JSONN_NUMBER,
  JSONN_STRING,
  JSONN_BEGIN_ARRAY,
  JSONN_END_ARRAY,
  JSONN_BEGIN_OBJECT,
  JSONN_END_OBJECT,
  JSONN_EOF,
  JSONN_UNEXPECTED,
} jsonn_type;

typedef enum {
  JSONN_LONG_RESULT,
  JSONN_DOUBLE_RESULT,
  JSONN_STRING_RESULT,
  JSONN_STREAM_RESULT,
  JSONN_ERROR_RESULT
} jsonn_result_type;

typedef struct {
  json_result_type type;
  int extra;
  union {
    int64_t long_number;
    double double_number;
    uint8_t *string;
    FILE *stream;
    jsonn_error error;
  } is;
} jsonn_result;

typedef struct {
  size_t at;
  char *message;
} jsonn_error;

typedef struct {
  void (*j_literal)(jsonn_parser, jsonn_type);
  void (*j_long)(jsonn_parser, int64_t);
  void (*j_double)(jsonn_parser, double);
  void (*j_string)(jsonn_parser, uin8_t *, size_t);
  void (*j_stream)(jsonn_parser, FILE *);
  void (*j_begin_array)(jsonn_parser);
  void (*j_end_array)(jsonn_parser);
  void (*j_begin_object)(jsonn_parser);
  void (*j_end_object)(jsonn_parser);
  int (*j_error)(jsonn_parser, jsonn_error);
} jsonn_callbacks;

typedef struct {
  uint8_t *current;
  uint8_t *last;
  uint8_t *write;
} jsonn_next_parser;

typedef struct {
  FILE *stream;
} jsonn_stream_parser;

typedef struct {
  int type;
  size_t stack_max;
  size_t stack_pointer;
  uint8_t *stack;
  union {
    jsonn_next_parser next;
    jsonn_stream_parser stream;
  } parser;
} *jsonn_parser;

json_parser jsonn_new(uint8_t* config);
void jsonn_free(jsonn_parser parser);

void jsonn_buffer(jsonn_parser parser, uint8_t *buffer, size_t length);
void jsonn_stream(jsonn_parser parser, FILE *stream);

jsonn_type jsonn_next(jsonn_parser parser, jsonn_result *result);
json_type jsonn_callback(jsonn_parser, jsonn_callbacks callbacks, jsonn_result *result);

