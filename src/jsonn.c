#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

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
  JSONN_LONG,
  JSONN_DOUBLE,
} jsonn_data_type;

typedef union {
  long long_value;
  double double_value;
} jsonn_data;

typedef struct {
  jsonn_data_type data_type;
  jsonn_data data;
} jsonn_value;

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

int ignore_comments;
int ignore_trailing_commas;

void consume_bom() {
  if(last_byte > 2 + current_byte
      && '\xEF' == *current_byte
      && '\xBB' == *(1 + current_byte)
      && '\xBF' == *(2 + current_byte)) {
    current_byte += 3;
  }
}

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

void consume_line_comment() {
  while(current_byte < last_byte && '\r' != *current_byte && '\n' != *current_byte)
    current_byte++;
}

void consume_block_comment() {
  while(1) {
    while(current_byte < last_byte && '*' != *current_byte)
      current_byte++;
    if('/' == *current_byte) return;
  }
}

void consume_whitespace() {
  while(current_byte < last_byte) {
    switch(*current_byte) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
      break;
    case '/':
      if(!ignore_comments) return;

      char next_byte = *(1 + current_byte); // safe as we are null terminated
      switch(next_byte) {
      case '/':
        current_byte += 2;
        consume_line_comment();
        break;
      case '*':
        current_byte += 2;
        consume_block_comment();
        break;
      default:
        return;
      }
    default:
        return;
    }
    current_byte++;
  }
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

jsonn_type parse_number(jsonn_value *result) {
  char *start = current_byte;
  char *long_end = NULL;
  char *double_end = NULL;
  errno = 0;
  long long_val = strtol(current_byte, &long_end, 10);
  if(errno) return JSONN_UNEXPECTED;
  double double_val = strtod(current_byte, &double_end);
  if(errno) return JSONN_UNEXPECTED;
  if(long_end < double_end) {
    // strold accepts all sorts on inputs that JSON doesn't
    // Nan, Infinity, hex digits and binary exponents
    // We reject those
    char *s = current_byte;
    while(s < double_end) {
      if(!strchr("0123456789+-.eE", *s)) return JSONN_UNEXPECTED;
    }
    current_byte = double_end;
    result->data_type = JSONN_DOUBLE;
    result->data.double_value = double_val;
  } else {
    current_byte = long_end;
    result->data_type = JSONN_LONG;
    result->data.long_value = long_val;
  }
  return JSONN_NUMBER;
}

int accept_escaped_unicode(int u, int u2) {
  int shift;

  if(u2 < 0) {
    // Basic Multilingual Plane
    if(u < 0x80) {
      // Ascii, just the byte
      return *write_byte++ = (char)u;
    } else if(u < 0x800) {
      // 2 bit UTF8, byte 1 
      // 110 and highest 5 bits
      *write_byte++ = (char)(0xC0 | ((u >> 6) & 0x1F));
      shift = 0;
    } else {
      // 3 bit UTF8, byte 1
      // 1110 and highest 4 bits
      *write_byte++ = (char)(0xE0 | ((u >> 12) & 0x0F));
      shift = 6;
    }
  } else {
    // Codes from other planes as UTF16 surrogate pair
    // Code point mapped from pair as
    // 110110yyyyyyyyyy 110111xxxxxxxxxx => 0x10000 + yyyyyyyyyyxxxxxxxxxx
    if( 0b00110110 == u >> 10 && 0b00110111 == u2 >> 10) {
      u = 0x10000 + ((u & 0x3FF) << 10) + (u2 & 0x3FF);
      // 4 bit UTF8, byte 1
      // 11110 and highest 3 bytes
      *write_byte++ = (char)(0xF0 | ((u >> 18) & 0x07));
      shift = 12;
    } else {
      return 0;
    }
  }
  // continuation bytes - high two bits '10' and next highest 6 bits from codepoint 
  while(shift >= 0) {
    *write_byte++ = (char)(0x80 | u >> shift & 0x3F);
    shift -= 6;
  }
  return 1;
}

int accept_utf8_chars() {
  int codepoint;
  int limit;
  int cont;

  if(0b00000110 == *current_byte >> 5) {
    codepoint = 0b00011111 & *current_byte;
    limit = 0x7F;
    cont = 1;
  } else if(0b00001110 == *current_byte >> 4) {
    codepoint = 0b00001111 & *current_byte;
    limit = 0x7FF;
    cont = 2;
  } else if(0b00011110 == *current_byte >> 3) {
    codepoint = 0b0000111 & *current_byte;
    limit = 0xFFFF;
    cont = 3;
  } else {
    return 0;
  }

  *write_byte++ = *current_byte++;
  for(int i = 0 ; i < cont ; i++) {
    if( 0b00000010 == *current_byte >> 6) {
      codepoint = (codepoint << 6) + (0b00111111 & *current_byte);
      *write_byte++ = *current_byte++;
    } else {
      return 0;
    }
  }
  return codepoint > limit;
}

int parse_4hexdig(char *prefix) {
  // This may be a speculative parse (looking for second \uXXXX after first \uXXXX)
  // We must be careful not to leave current_buffer in the middle of a failed parse
  int value = 0;
  char *value_byte = current_byte;
  if(prefix) {
    while(*prefix) {
      if(*value_byte++ != *prefix++) return -1;
    }
  }
  for(int i = 0 ; i < 4 ; i++) {
    int v = *current_byte++;
    if(v >= '0' && v <= '9') {
      v -= '0';
    } else if(v >= 'A' && v <= 'F') {
      v -= 'A' - 10;
    } else if(v >= 'a' && v <= 'f') {
      v -= 'a' - 10;
    } else {
      return -1;
    }
    value = (value << 4) | v;
  }
  // Parsed ok, so set current_byte
  current_byte = value_byte;
  return value;
}

jsonn_type parse_string(jsonn_value *result) {
  // We modify the buffer as decoding escapes takes up less space
  // and even if there is no escape then the null byte can overwirte the closing "
  // After an escape we end up copying bytes but we avoid copying leading ascii bytes

  // We know we have the leading quote, so skip it
  current_byte++;
  char *start_byte = current_byte;
  
  // Skip leading ascii
  while(0x20 < *current_byte
        && '"' != *current_byte
        && '\\' != *current_byte
        && 0x80 > *current_byte)
    current_byte++;

  write_byte = current_byte;
  while(current_byte < last_byte) {

    // Next byte will be ", /, ascii control or UTF-8
    if('"' == *current_byte) {
      *write_byte = '\0';
      result->data_type = JSONN_LONG;
      result->data.long_value = write_byte - start_byte;
      return JSONN_STRING;
    }

    if('\\' == *current_byte) {
      char c = '\0';
      current_byte++;
      switch(*current_byte) {
      case '"':
      case '\\':
      case '/':
        c = *current_byte;
        break;

      case 'b':
        c = '\b';
        break;

      case 'f':
        c = '\f';
        break;

      case 'n':
        c = '\n';
        break;

      case 'r':
        c = '\r';
        break;

      case 't':
        c = '\t';
        break;

      case 'u':
        // \uXXXX [\uXXXX]
        // more complex and single char so we do not set variable c
        // but let accept_escaped_unicode copy the required bytes
        current_byte++;
        int u = parse_4hexdig(NULL);
        if(u < 0) return JSONN_UNEXPECTED;
        int u2 = parse_4hexdig("\\u");
        if(!accept_escaped_unicode(u, u2)) return JSONN_UNEXPECTED;
        break;

      default:
        return JSONN_UNEXPECTED;
      }
      if(c) {
        *write_byte++ = c;
        current_byte++;
      }

    } else if(0x20 > *current_byte) {
      return JSONN_UNEXPECTED;

    } else {
      // UTF-8 expected
      if(!accept_utf8_chars()) return JSONN_UNEXPECTED;
    }

    // Ready for more ascii chars, but now we have to copy them
    while(0x20 < *current_byte
          && '"' != *current_byte
          && '\\' != *current_byte
          && 0x80 > *current_byte)
      *write_byte++ = *current_byte++;
  }

}

jsonn_type parse_name(jsonn_value *result) {
  if('"' == *current_byte) {
    current_byte++;
    return parse_string(result);
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

