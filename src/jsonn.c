#include <stdlib.h>

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
  JSONN_COMMENT_BLOCK,
  JSONN_COMMENT_LINE,
  JSONN_UNEXPECTED,
} jsonn_type;

typedef enum {
  JSONN_LONG,
  JSONN_DOUBLE,
} jsonn_data_type;

typedef union {
  long long_data;
  double double_data;
} jsonn_data;

typedef struct {
  jsonn_data_type data_type;
  jsonn_data data;
} jsonn_value;

typedef enum {
  JSONN_VALUE,
  JSONN_VALUE_NEXT,
  JSONN_NAME,
  JSONN_NAME_NEXT,
} jsonn_next;

jsonn_next next;
  

#define MAX_NESTING 1024
char nesting_stack[MAX_NESTING];
int nesting_pointer;
int nesting_max = MAX_NESTING;

unsigned char *input_bytes;
unsigned char *last_byte;
unsigned char *current_byte;

unsigned char *string_bytes;
unsigned char *last_string_byte;
unsigned char *current_string_byte;

int ignore_comments;
int ignore_trailing_commas;

void skip_bom() {
  if(last_byte > 2 + current_byte
      && '\xEF' == *current_byte
      && '\xBB' == *(1 + current_byte)
      && '\xBF' == *(2 + current_byte)) {
    current_byte += 3;
  }
}

int jsonn_parse_init(unsigned char *input_buf, unsigned input_max, unsigned char *string_buf, unsigned string_max, unsigned max_nest_depth, unsigned is_jsonc) {
  assert(max_nest_depth <= MAX_NESTING);
  assert(is_jsonc == 0 || is_jsonc == 1);

  input_bytes = input_buf;
  current_byte = input_bytes;
  last_byte = input_bytes + input_max;
  *last_byte = '\0'; // Allows us to test 2 characters without worrying about end of buffer

  string_bytes = string_buf;
  last_string_bytes = string_bytes + string_max;

  nesting_max = max_nest_depth;
  ignore_comments = ignore_trailing_commas = is_jsonc;

  skip_bom(); // unlikely but JSON spec says it may be there and should be ignored
}

void skip_line_comment() {
  while(current_byte < last_byte && '\r' != *current_byte && '\n' != *current_byte++)
    ;
}

void skip_block_comment() {
  while(1) {
    while(current_byte < last_byte && '*' != *current_byte++)
      ;
    if('/' == *current_byte) return;
  }
}

void skip_whitespace() {
  while(current_byte < last_byte) {
    unsigned char this_byte = *current_byte;
    switch(this_byte) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
      break;
    case '/':
      if(!ignore_comments) return;

      unsigned char next_byte = *(1 + current_byte); // safe as we are null terminated
      switch(next_byte) {
      case '/':
        current_byte += 2;
        skip_line_comment();
        break;
      case '*':
        current_byte += 2;
        skip_block_comment();
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
  push_next(JSONN_OBJECT_MEMBER_OPTIONAL);
  return JSONN_BEGIN_OBJECT;
}

jsonn_type end_object() {
  current_byte++;
  pop_next();
  return JSONN_END_OBJECT;
}

jsonn_type begin_array() {
  current_byte++;
  push_next(JSONN_ARRAY_VALUE_OPTIONAL);
  return JSONN_BEGIN_ARRAY;
}

jsonn_type end_array() {
  current_byte++;
  pop_next();
  return JSONN_END_ARRAY;
}

jsonn_type parse_literal(unsigned char *literal, jsson_type type) {
  // Already matched first character of literal to get here so we can skip that one
  unsigned char *s1 = literal + 1;
  unsigned char *s2 = current_byte + 1;
  while(match < last_byte && *s1 && *s1 == *s2)
    ;
  // If we are at the null at the end of literal then we found it
  if(!*s1) {
    current_byte = s2;
    return type;
  }
  return JSONN_UNEXPECTED;
}

jsonn_parse parse_number(jsonn_value *result) {
  char *start = current_byte;
  char *long_end = *double_end = 0;
  errno = 0;
  long long long_val = strtoll(current_byte, &long_end, 10);
  if(errno) return JSONN_UNEXPECTED;
  long double double_val = strold(current_byte, &double_end);
  if(errno) return JSONN_UNEXPECTED;
  if(long_end < double_end) {
    // strold accepts all sorts on inputs that JSON doesn't
    // Nan, Infinity, hex digits and binary exponents
    // We reject those
    char *s = current_byte;
    while(s < double_end) {
      if(!strch("0123456789+-.eE", *s)) return JSONN_UNEXPECTED;
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

int accept_string_char(char c) {
  if(current_string_byte < last_string_byte) {
    current_byte++;
    *current_string_byte = c;
    current_string_byte++;
    return 1;
  } else {
    return 0;
}

int accept_string_escaped(int u1, int u2) {
  if(u2 < 0) 
    return accept_string_codepoint(u1);
  else
    return accept_string_utf16(u1, u2);
}

int accept_string_codepoint(int u) {
  if(u < 0x80) {
    return accept_string_char((char)u);
  } else if(u < 0x800) {
    return accept_string_char((char)((u >> 6) & 0x1F) | 0xC0)
      && accept_string_char((char)(u & 0x3F) | 0x80);
  } else {
    return accept_string_char((char)((u >> 12) & 0x0F) | 0xE0)
      && accept_string_char((char)((u >> 6) & 0x3F) | 0x80)
      && accept_string_char((char)(u & 0x3F) | 0x80);
  }

int accept_string_utf16(int u1, int u2) {
  if( 0xD800 != 0xD800 & u1 || 0xDC00 != 0xDC00 & u2) return 0;
  int u = 0x1000 + ((u1 & 0x3FF) << 10) + (u2 & 0x3FF);
  return
    accept_string_char((char)((u >> 18) & 0x07) | 0xF0)
    && accept_string_char((char)((u >> 12) & 0x3F) | 0x80)
    && accept_string_char((char)((u >> 6) & 0x3F) | 0x80)
    && accept_string_char((char)(u & 0x3F) | 0x80);
}


jsonn_type = parse_string(jsonn_value *result) {
  // We know we have the leading quote, so skip it
  current_byte++;
  char *current_string_byte = string_bytes;
  char *last_string_byte = string_bytes + max_string_bytes;
  while(current_byte < last_byte && current_string_byte < last_string_byte) {
    if('"' == *current_byte) {
      *current_string_byte = '\0';
      result->data_type = JSONN_LONG;
      result->data.long_data = current_string_byte - string_bytes;
      return JSONN_STRING;
    }
    if('\\' == *current_byte) {
      current_byte++;
      switch(*current_byte) {
      case '"':
      case '\\':
      case '/':
        accept = *current_byte;
        break;

      case 'b':
        accept = '\b';
        break;

      case 'f':
        accept = '\f';
        break;

      case 'n':
        accept = '\n';
        break;

      case 'r':
        accept = '\r';
        break;

      case 't':
        accept = '\t';
        break;

      case 'u':
        accept = -1;
        break;

      default:
        return JSONN_UNEXPECTED;
      }
      if(accept > 0) {
        accept_string_char(accept);

      } else {
        int n1 = parse_hexdig();
        if(n1 < 0) return JSONN_UNEXPECTED;
        int n2 = parse_hexdig();
        if(!accept_string_escaped(n1, n2)) return JSONN_UNEXPECTED;
      }
    } else {

}

jsonn_type parse_value(jsonn_value *result) {
  if(current_byte < last_byte) {
    unsigned_char this_byte = *current_byte;
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
    unsigned_char this_byte = *current_byte;
    if(']' == *current_byte) {
      current_byte++;
      return end_array();
    }
    return JSONN_UNEXPECTED;
  }

jsonn_type parse_array_terminator(jsonn_value *result) {
  if(current_byte < last_byte) {
    unsigned_char this_byte = *current_byte;
    if('}' == *current_byte) {
      current_byte++;
      return end_object();
    }
    return JSONN_UNEXPECTED;
  }


jsonn_next consume_list_value_separator() {
  if(current_byte < last_byte) {
    unsigned_char this_byte = *current_byte;
    switch(this_byte) {
    case ',':
      current_byte++;
      return ignore_trailing_commas ? JSONN_ARRAY_VALUE_OPTIONAL : JSONN_ARRAY_VALUE;
    case ']':
      // not separator so don't consume
      return JSONN_ARRAY_TERMINATOR;
    }
  }
  return JSON_UNEXPECTED;
}

jsonn_next consume_pair_value_separator() {
  if(current_byte < last_byte) {
    unsigned_char this_byte = *current_byte;
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

jsonn_next consume_name_separator() {
  if(current_byte < last_byte) {
    unsigned_char this_byte = *current_byte;
    if(':' == this_byte) {
      current_byte++;
      return JSONN_NAME_VALUE;
    }
  }
  return JSONN_UNEXPECTED;
}

jsonn_type jsonn_parse_next(*jsonn_value result) {
  while(1) {
    skip_white_space();
    optional = 1; // are values/pairs required?
    switch(next) {

    case JSONN_ARRAY_VALUE:
      optional = 0;
      // fall through
    case JSONN_ARRAY_VALUE_OPTIONAL:
      ret = parse_value(result);
      if(valid(ret)) {
        next = JSON_ARRAY_VALUE_SEPARATOR;
        return ret;
      } else if (!optional) {
        return ret;
      }
      next = JSON_ARRAY_TERMINATOR;
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
      if(valid(ret)) {
        next = JSONN_NAME_SEPARATOR;
        return ret;
      } else if(!optional) {
        return ret;
      }
      next = JSON_OBJECT_TERMINATOR;
      break;

    case JSONN_NAME_SEPARATOR:
        next = consume_name_separator();
        break;

    case JSON_NAME_VALUE:
        ret = parse_value(result);
        if(valid(ret)) {
          next = JSONN_OBJECT_VALUE_SEPARATOR;
        }
        return ret;

    case JSONN_OBJECT_VALUE_SEPARATOR:
      next = consume_object_value_separator();
      break;

    case JSONN_OBJECT_TERMINATOR:
      return parse_object_terminator(result);

    default:
      return parse_failure(result);
    }
}




int jsonn_write_init(FILE *stream, int print_eol, int print_indent) {
ST}

int jsonn_write_value(jsonn_type type) {
}

int jsonn_write_long(long long number) {
}

int jsonn_write_double(long double number) {
}

int jsonn_write_string(char *string, int char_count, int iscomplete) {
}

int jsonn_write_comment(jsonn_type type, char *text) {
}
