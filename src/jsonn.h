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
  JSONN_LONG_VALUE,
  JSONN_DOUBLE_VALUE,
  JSONN_STRING_VALUE
} jsonn_value_type;

typedef struct {
  int info;
  union {
    long long_value;
    double double_value;
    uint8_t *string_value;
  } is;
} jsonn_value;

typedef struct {
  jsonn_value_type type;
  jsonn_value value;
} jsonn_result;


int jsonn_parse_init(unit8_t *json_text,  size_t json_length, int is_jsonc);
jsonn_type jsonn_parse_next(jsonn_result *result);
