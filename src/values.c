#include <stdint.h>

#include "jsonn.h"
#include "values.h"
#include "utf8.h"

// TODO:
// distinguish between unicode that has been encoded in the utf-8 byte stream#
// and that encoded in \uXXXX escapes as the validation is different
// we encode the \uXXXX so can enfore the correct rules
// the byte sequences in the utf-8 could be anything!
//
// Also, tidy/consisent naming/consistent returns etc
int ignore_comments = 0;
int ignore_trailing_commas = 0;
int replace_illegal_utf8 = 0;

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

int skip_sequence(uint8_t *sequence) {
  uint8_t *s = current_byte;
  while(*sequence && *s++ == *sequence++)
    ;

  if(*sequence) return 0;

  current_byte = s;
  return 1
}

void write_byte(uint8_t byte) {
  *write_byte++ = byte;
  current_byte++;
}

int parse_unicode_escapes() {
  // could have a single unicode (non surrogate)
  // or a surrogate pair
  // so we may have to do some looking ahead ...

  // We are at the start of a 4 character hex sequence
  int u = parse_4hexdig();
  if(u < 0) 
    return 0;

  if(is_first_surrogate(u)) {
    // look ahead to see if we have a surrogate pair
    // if not we must reset the current byte
    uint8_t *m = current_byte;
    int u2 = skip_sequence("\\u") ? parse_4hexdig() : -1;
    if(is_second_surrogate(u2)) {
      u = surrogate_pair_to_codepoint(u, u2);
    } else {
      // This is not the surrogate you are looking for ...
      current_byte = m;
    }
  }
  return u;
}

int parse_4hexdig() {
  int value = 0;
  char *value_byte = current_byte;
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

jsonn_type long_result(long long_value, jsonn_result *result) {
  result->value_type = JSONN_LONG_VALUE;
  result->value.is.long_value = long_value;
  return JSONN_LONG;
}

jsonn_type double_result(double double_value, jsonn_result *result) {
  result->value_type = JSONN_DOUBLE_VALUE;
  result->value.is.double_value = double_value;
  return JSONN_DOUBLE;
}

jsonn_type string_result(utf8_t *string_value, int string_length, jsonn_result *result) {
  result->value_type = JSONN_STRING_VALUE;
  result->value.is.string_value = string_value;
  result->value.info = string_length;
  return JSONN_STRING;
}

// parse_name doesn't know if it has opening " whereas parse_string does
// so we have to check
// TODO: inconsistent interface
jsonn_type parse_name(jsonn_value *result) {
  if('"' == *current_byte) {
    return parse_string(result);
  }
  return JSONN_UNEXPECTED;
}


int can_replace_illformed_utf8() {
  if(replace_illformed_sequence && room_for_replacement()) {
    write_replacement_character();
    return 1;
  } else {
    return 0;
  }
}

/*
 * Parse JSON string into utf-8
 *
 * Returns JSONN_STRING if the string was converted to utf8
 *         JSONN_UNEXPECTED if the string could not be converted 
 * The actual string is set in jsonn_result
 *
 * The returned string is in the same place as it is in the JSON string
 * except that it is NULL terminated (usually overwriting the closing "s)
 *
 * All leading ascii bytes can be skipped
 * Escape sequences are translated to actual bytes, which will be
 * shorter than the JSON representation.  
 * After an escape sequence bytes will be copied rather than skipped.
 *
 * Any non-ascii utf-8 sequences are checked to see if they are well formed.
 * If illformed, and the replace_illformed_utf8 flag is not set, then 
 * JSONN_UNEXPECTED will be returned.
 * If the flag is set then illformed utf-8 will be replaced
 * with the utf-8 encoding of the Unicode Replacement Character.
 *
 * As the encoded replacement character is 3 bytes and illformed bytes can
 * be 1-4 bytes there is a chance that multiple replacements will overflow
 * the original JSON string.  If situation arrises then no rreplacement
 * will be made and JSONN_UNEXPECTED will be returned.
 */
jsonn_type parse_string(int8_t *utf8, size_t count, jsonn_result *result) {
  int8_t *current_byte = utf8;
  int8_t *write_byte = utf8;
  int8_t *last_byte = utf8 + count;
  int illformed_utf8 = 0;

  // Skip leading ascii
  while(0x20 < *current_byte
        && '"' != *current_byte
        && '\\' != *current_byte
        && 0x80 > *current_byte)
    current_byte++;

  while(current_byte < last_byte) {
    // Next byte will be ", \, ascii control or UTF-8

    if('"' == *current_byte) {
      current_byte++;
      *write_byte = '\0';
      return string_result(utf8, write_byte - utf8, result);
    }

    if('\\' == *current_byte) {
      char c = '\0';
      current_byte++;
      switch(*current_byte) {
      case '"':
      case '\\':
      case '/':
        accept_byte(*current_byte);
        break;

      case 'b':
        accept_byte('\b');
        break;

      case 'f':
        accept_byte('\f');
        break;

      case 'n':
        accept_byte('\n');
        break;

      case 'r':
        accept_byte('\r');
        break;

      case 't':
        accept_byte('\t');
        break;

      case 'u':
        // \uXXXX [\uXXXX]
        current_byte++;
         = !parse_unicode_escapes(write_byte);
        break;

      default:
        return JSONN_UNEXPECTED;
      }

    } else if(0x20 > *current_byte) {
      return JSONN_UNEXPECTED;

    } else {
      // UTF-8
      int count = write_utf8_sequence(current_byte, write_byte);
      if(count > 0) {
        current_byte += count;
        write_byte += count;
      } else {
        current_byte += -count;
        illformed_ut8 = 1;
      }
    }

    if(illformed_utf8) {
      if(can_replace_illformed_utf8(current_byte - write_byte)) {
        write_byte += write_replacement_character(write_byte);
        illformed_utf8 = 0;
      } else {
        return JSONN_UNEXPECTED;
      }
    }

    // Ready for more ascii chars, but now we have to copy them
    while(0x20 < *current_byte
          && '"' != *current_byte
          && '\\' != *current_byte
          && 0x80 > *current_byte)
      *write_byte++ = *current_byte++;
  }

}

/*
 * Parse JSON number as double or long
 * Return JSONN_DOUBLE or JSONN_LONG and set value into jsonn_result
 * Return JSONN_UNEXPECTED if string cannot be parsed as a number
 *
 * JSON doesn't permit leading 0s or hex numbers but strtod does
 * so validate first.
 *
 * If double value is equal to the long value then return the long value
 */
jsonn_type parse_number(const int8_t *utf8, jsonn_result *result) {
  uint8_t *current_byte = utf8;
  uint8_t *double_end;
  double double_val;
  long long_val;

  if('-' == *current_byte)
    current_byte++;

  // Leading 0 implies 0 or 0.fraction
  // So if no following . the result is 0
  if('0' == *current_byte && '.' != *(current_byte + 1)) {
    current_byte++;
    return long_result(0, result);
  }

  errno = 0;
  double_val = strtod(utf8, &double_end);
  if(errno) return JSONN_UNEXPECTED;

  current_byte = double_end;
  long_val = (long)double_val;
  return (double_val == long_val)
    ? long_result(long_value, result)
    : double_result(double_value, result);
}

