#pragma once

#include <stddef.h>

#include "jsonn.h"

typedef enum {
        // Don't change as the first two values are used for stack bit
        PARSE_OBJECT_MEMBER_SEPARATOR = 0,
        PARSE_ARRAY_VALUE_SEPARATOR = 1,
        PARSE_OBJECT_MEMBER_OPTIONAL,
        PARSE_ARRAY_VALUE_OPTIONAL,
        PARSE_OBJECT_MEMBER,
        PARSE_ARRAY_VALUE,
        PARSE_OBJECT_NAME_SEPARATOR,
        PARSE_OBJECT_MEMBER_VALUE,
        PARSE_OBJECT_TERMINATOR,
        PARSE_ARRAY_TERMINATOR,
        PARSE_NAME,
        PARSE_VALUE,
        PARSE_EOF,
        PARSE_ERROR
} parse_next;

struct jsonn_parser_s {
        uint8_t *start;
        uint8_t *current;
        uint8_t *last;
        uint8_t *write;
        parse_next next;
        uint16_t flags;
        jsonn_value result;
        size_t stack_size;
        size_t stack_pointer;
        uint8_t *stack;
};

typedef struct jsonn_parser_s *jsonn_parser;
