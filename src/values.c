
// TODO:
// distinguish between unicode that has been encoded in the utf-8 byte stream#
// and that encoded in \uXXXX escapes as the validation is different
// we encode the \uXXXX so can enfore the correct rules
// the byte sequences in the utf-8 could be anything!
//
// Also, tidy/consisent naming/consistent returns etc
int ignore_comments = 0;
int ignore_trailing_commas = 0;

void consume_bom() {
  if(last_byte > 2 + current_byte
      && '\xEF' == *current_byte
      && '\xBB' == *(1 + current_byte)
      && '\xBF' == *(2 + current_byte)) {
    current_byte += 3;
  }
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

int skip_sequence(uint8_t *sequence) {
  uint8_t *s = current_byte;
  while(*sequence && *s++ == *sequence++)
    ;

  if(*sequence) return 0;

  current_byte = s;
  return 1
}

int is_surrogate(int u) {
  return u >= 0xDC00 && u <= 0xDFFF;
}

int is_valid_codepoint(int u) {
  return u <= 0x10FFFF && !is_surrogate(u);
}

void accept_byte(uint8_t byte) {
  *write_byte++ = byte;
  current_byte++;
}

// Write the Unicode replacement character U+FFFD as UTF-8
void accept_replacement_character() {
  *write_byte++ = 0xEF;
  *write_byte++ = 0xBD;
  *write_byte++ = 0xBD;
}

void accept_codepoint(int u) {
  // Invalid codepoints are substituted with
  // the replacement character
  if(u < 0x80) {
    // Ascii, just the byte
    *write_byte++ = (char)u;
    return;
  } else if(u < 0x800) {
    // 2 bit UTF8, byte 1 is
    // 110 and highest 5 bits
    *write_byte++ = (char)(0xC0 | ((u >> 6) & 0x1F));
    shift = 0;
  } else if(is_surrogate(u)) {
    // UTF-16 surrogates are not legal Unicode
    // Substitute with the replacement character
    accept_replacement_character();
    return;
  } else if(u < 0x10000) {
    // 3 bit UTF8, byte 1 is
    // 1110 and highest 4 bits
    *write_byte++ = (char)(0xE0 | ((u >> 12) & 0x0F));
    shift = 6;
  } else if(u <= 0x10FFFF) {
    // 4 bit UTF8, byte 1 is
    // 11110 and highest 3 bytes
    *write_byte++ = (char)(0xF0 | ((u >> 18) & 0x07));
    shift = 12;
  } else {
    // >0x10FFFF not legal Unicode
    // Substitute with the replacement character
    accept_replacement_character();
    return;
  }
  // Now the continuation bytes
  // high two bits '10' and next highest 6 bits from codepoint 
  while(shift >= 0) {
    *write_byte++ = (char)(0x80 | u >> shift & 0x3F);
    shift -= 6;
  }
}

void accept_surrogate_pair(int u1, int u2) {
  // Codes from other planes as UTF16 surrogate pair
  // Code point mapped from pair as
  // 110110yyyyyyyyyy 110111xxxxxxxxxx 
  //   => 0x10000 + yyyyyyyyyyxxxxxxxxxx
  if( 0xD800 == u1 & 0xFC00 && 0xDC00 == u2 & 0xFC00) {
    int u = 0x10000 + ((u1 & 0x3FF) << 10) + (u2 & 0x3FF);
    accept_codepoint(u);
  } else {
    // Invalid pair
    // Substitute with the replacement character
    accept_replacement_character();
  }
}

int parse_unicode_escapes() {
  // could have a single unicode (non surrogate)
  // or a surrogate pair
  // so we may have to do some looking ahead ...

  // We are at the start of a 4 character hex sequence
  int u = parse_4hexdig();
  if(u < 0) return 0;

  if(is_surrogate(u)) {
    // look ahead to see if we have a surrogate pair
    // if not we must reset the current byte
    uint8_t *m = current_byte;
    int u2 = skip_sequence("\\u") ? parse_4hexdig() : -1;
    if(is_surrogate(u2)) {
      accept_surrogate_pair(u, u2);
      return 1;
    } else {
      // This is not the surrogate you are looking for ...
      current_byte = m;
    }
  }
  accept_codepoint(u);
  return 1
}

// The strategy of writing back strings to the original data buffer
// makes sense with well formed UTF-8 as everything in the decoded
// string is the same size or smaller than the source
// Unless there are ill-formed UTF-8 sequences.
// Ill-formed sequences of code unit(s) (1-4 bytes) are replaced
// by the (well named) replacement character which is 3 bytes.
// A pathalogically ill-formed sequence could easily take up
// more space with replacement characters than the original
// We can't fix it at this level, only write the bytes once
// we detect a valid sequence
//
// This function does not deal with ascii chars, just multibyte
int accept_utf8_chars() {
  int codepoint;
  int limit;
  int cont;

  uint8_t *start_byte = current_byte;

  uint8_t byte = *current_byte++;
  if(0xC0 == byte & 0xE0) {
    // 2 byte UTF8
    codepoint = byte & 0x1F;
    limit = 0x7F;
    cont = 1;
  } else if(0xE0 == byte & 0xF0) {
    // 3 byte UTF8
    codepoint = byte & 0x0F;
    limit = 0x7FF;
    cont = 2;
  } else if(0xF0 == byte & 0xF8) {
    // 4 byte UTF8
    codepoint = byte & 0x07;
    limit = 0xFFFF;
    cont = 3;
  } else {
    return 0;
  }

  for(int i = 0 ; i < cont ; i++) {
    byte = *current_byte++;
    if( 0x80 == byte & 0xC0) {
      codepoint = (codepoint << 6) + (byte & 0x3F);
    } else {
      return 0;
    }
  }
  if(codepoint <= limit || !is_valid_codepoint(u))
    // overlong encoding or invalid codepoint
    return 0;
  
  // we have a valid UTF-8 sequence so we can write it
  while(start_byte < current_byte)
    *write_byte++ = *start_byte++;

  return 1;
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
  return JSONN_NUMBER;
}

jsonn_type double_result(double double_value, jsonn_result *result) {
  result->value_type = JSONN_DOUBLE_VALUE;
  result->value.is.double_value = double_value;
  return JSONN_NUMBER;
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


jsonn_type parse_string(jsonn_result *result) {
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
      current_byte++;
      *write_byte = '\0';
      return string_result(start_byte, write_byte - start_byte, result);
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
        if(!parse_unicode_escapes())
          return JSONN_UNEXPECTED;
        
        break;

      default:
        return JSONN_UNEXPECTED;
      }

    } else if(0x20 > *current_byte) {
      return JSONN_UNEXPECTED;

    } else {
      // UTF-8 expected
      // If rejected then write a replacement character
      // IF THERE IS ROOM!
      if(!accept_utf8_chars()) {
        // buffer overflow!!!
        if(current_byte - write_byte < 3) return JSONN_UNEXPECTED;
        accept_replacement_character();
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
 * Return JSONN_NUMBER and set double/long value into jsonn_result
 *
 * JSON doesn't permit leading 0s or hex numbers but strtod does
 * so validate first.
 *
 * If double value is equal to (long)double value then return the long value
 */
jsonn_type parse_number(jsonn_result *result) {
  uint8_t *double_end;
  uint8_t *start_byte = current_byte;
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
  double_val = strtod(start_byte, &double_end);
  if(errno) return JSONN_UNEXPECTED;

  current_byte = double_end;
  long_val = (long)double_val;
  if(double_val == long_val) {
    return long_result(long_value, result);
  } else {
    return double_result(double_value, result);
  }
}

