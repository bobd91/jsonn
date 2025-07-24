#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jsonn.h"
#include "strings.h"
#include "numbers.h"

typedef enum {
  // Don't change these two values as the bit values are used for nesting
  JSONN_OBJECT_MEMBER_OPTIONAL = 0,
  JSONN_ARRAY_VALUE_OPTIONAL = 1,
  JSONN_OBJECT_MEMBER,
  JSONN_ARRAY_VALUE,
  JSONN_OBJECT_MEMBER_SEPARATOR,
  JSONN_OBJECT_NAME_SEPARATOR,
  JSONN_OBJECT_MEMBER_VALUE,
  JSONN_OBJECT_TERMINATOR,
  JSONN_ARRAY_TERMINATOR,
  JSONN_ARRAY_VALUE_SEPARATOR,
  JSONN_VALUE,
  JSONN_VALUE_NEXT,
  JSONN_NAME,
  JSONN_NAME_NEXT,
  JSONN_ERROR
} jsonn_next;

jsonn_next next;
  

#define MAX_NESTING 1024
char nest_stack[MAX_NESTING / 8];
int nest_level;
int nest_max = MAX_NESTING;

char *input_bytes;
char *last_byte;
char *current_byte;
char *write_byte;

jsonn_next pop_next() {
  if(--nest_level < 0) return JSONN_ERROR;
  int next = 0x01 & nest_stack[nest_level >> 3] >> (nest_level & 0x07);
  return next;
}

jsonn_next push_next(jsonn_next next) {
  // Only allow JSONN_[OBJECT|ARRAY]_VALUE_OPTIONAL   
  assert(!next >> 1);
  if(nest_level >= nest_max) return JSONN_ERROR;
  nest_stack[nest_level >> 3] |= next << (nest_level & 0x07);
  nest_level++;
  return next;
}

int jsonn_parse_init(char *input_buf, unsigned input_max) {
  input_bytes = input_buf;
  current_byte = input_bytes;
  last_byte = input_bytes + input_max;
  *last_byte = '\0'; // Allows us to test 2 characters without worrying about end of buffer

  consume_bom(); // unlikely but JSON spec says it may be there and should be ignored
}

jsonn_type begin_object() {
  current_byte++;
  if(push_next(JSONN_OBJECT_MEMBER_OPTIONAL) < 0) return JSONN_UNEXPECTED;
  return JSONN_BEGIN_OBJECT;
}

jsonn_type end_object() {
  current_byte++;
  if(pop_next() < 0) return JSONN_UNEXPECTED;
  return JSONN_END_OBJECT;
}

jsonn_type begin_array() {
  current_byte++;
  if(push_next(JSONN_ARRAY_VALUE_OPTIONAL) < 0) return JSONN_UNEXPECTED;
  return JSONN_BEGIN_ARRAY;
}

jsonn_type end_array() {
  current_byte++;
  if(pop_next() < 0) return JSONN_UNEXPECTED;
  return JSONN_END_ARRAY;
}

jsonn_type parse_literal(char *literal, jsonn_type type) {
  // Already matched first character of literal to get here so we can skip that one
  char *s1 = literal;
  char *s2 = current_byte;
  while(*++s1 == *++s2 && *s1)
    ;
  // If we are at the null at the end of literal then we found it
  if(!*s1) {
    current_byte = s2;
    return type;
  }
  return JSONN_UNEXPECTED;
}

jsonn_type parse_value(jsonn_value *result) {
  if(current_byte < last_byte) {
    char this_byte = *current_byte;
    switch(this_byte) {
    case '"':
      return parse_string(result);
    case '{':
      return begin_object();
    case '[':
      return begin_array();
    case 'n':
      return parse_literal("null", JSONN_NULL);
    case 'f':
      return parse_literal("false", JSONN_FALSE);
    case 't':
      return parse_literal("true", JSONN_TRUE);
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return parse_number(result);
    }
  }
  return JSONN_UNEXPECTED;
}

jsonn_type parse_array_terminator(jsonn_value *result) {
  if(current_byte < last_byte) {
    char this_byte = *current_byte;
    if(']' == *current_byte) {
      current_byte++;
      return end_array();
    }
    return JSONN_UNEXPECTED;
  }
}

jsonn_type parse_object_terminator(jsonn_value *result) {
  if(current_byte < last_byte) {
    char this_byte = *current_byte;
    if('}' == *current_byte) {
      current_byte++;
      return end_object();
    }
    return JSONN_UNEXPECTED;
  }
}


jsonn_next consume_array_value_separator() {
  if(current_byte < last_byte) {
    char this_byte = *current_byte;
    switch(this_byte) {
    case ',':
      current_byte++;
      return ignore_trailing_commas ? JSONN_ARRAY_VALUE_OPTIONAL : JSONN_ARRAY_VALUE;
    case ']':
      // not separator so don't consume
      return JSONN_ARRAY_TERMINATOR;
    }
  }
  return JSONN_UNEXPECTED;
}

jsonn_next consume_object_member_separator() {
  if(current_byte < last_byte) {
    char this_byte = *current_byte;
    switch(this_byte) {
    case ',':
      current_byte++;
      return ignore_trailing_commas ? JSONN_OBJECT_MEMBER_OPTIONAL : JSONN_OBJECT_MEMBER;
    case '}':
      // not separator so don't consume
      return JSONN_OBJECT_TERMINATOR;
    }
  }
  return JSONN_UNEXPECTED;
}

jsonn_next consume_object_name_separator() {
  if(current_byte < last_byte) {
    char this_byte = *current_byte;
    if(':' == this_byte) {
      current_byte++;
      return JSONN_OBJECT_MEMBER_VALUE;
    }
  }
  return JSONN_UNEXPECTED;
}

int is_valid(jsonn_type type) {
  return type != JSONN_UNEXPECTED;
}

int is_value(jsonn_type type) {
  return type == JSONN_FALSE
    || type == JSONN_NULL
    || type == JSONN_TRUE
    || type == JSONN_NUMBER
    || type == JSONN_STRING;
}

jsonn_type jsonn_parse_next(jsonn_value *result) {
  jsonn_type ret;
  while(1) {
    consume_whitespace();
    int optional = 1; // are values/pairs required?
    switch(next) {

    case JSONN_ARRAY_VALUE:
      optional = 0;
      // fall through
    case JSONN_ARRAY_VALUE_OPTIONAL:
      ret = parse_value(result);
      if(is_valid(ret)) {
        if(is_value(ret))
          next = JSONN_ARRAY_VALUE_SEPARATOR;
        return ret;
      } else if (!optional) {
        return ret;
      }
      next = JSONN_ARRAY_TERMINATOR;
      break;
      
    case JSONN_ARRAY_VALUE_SEPARATOR:
      next = consume_array_value_separator();
      break;

    case JSONN_ARRAY_TERMINATOR:
      return parse_array_terminator(result);

    case JSONN_OBJECT_MEMBER:
      optional = 0;
      // fall through
    case JSONN_OBJECT_MEMBER_OPTIONAL:
      ret = parse_name(result);
      if(is_valid(ret)) {
        next = JSONN_OBJECT_NAME_SEPARATOR;
        return ret;
      } else if(!optional) {
        return ret;
      }
      next = JSONN_OBJECT_TERMINATOR;
      break;

    case JSONN_OBJECT_NAME_SEPARATOR:
        next = consume_object_name_separator();
        break;

    case JSONN_OBJECT_MEMBER_VALUE:
        ret = parse_value(result);
        if(is_valid(ret)) {
          if(is_value(ret)) {
            next = JSONN_OBJECT_MEMBER_SEPARATOR;
          }
          return ret;
        }
        return ret;

    case JSONN_OBJECT_MEMBER_SEPARATOR:
      next = consume_object_member_separator();
      break;

    case JSONN_OBJECT_TERMINATOR:
      return parse_object_terminator(result);

    default:
      return JSONN_UNEXPECTED;
    }
  }
}

