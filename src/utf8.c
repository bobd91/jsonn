/*
 * jsonn - a simple JSON parser
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * utf8.c
 *   validating UTF-8 sequences in JSON strings
 *   encoding Unicode codepoints into valid UTF-8 byte sequences
 *   identifying UTF-16 surrogare pairs and converting to codepoint
 */

#include <stdint.h>
#include <utf.h>

#define BYTE_ORDER_MARK "\xEF\xBB\xBF"
#define REPLACEMENT_CHARACTER "\xEF\xBD\xBD"

#define SURROGATE_MIN           0xD800
#define SURROGATE_MAX           0xDFFF
#define SURROGATE_OFFSET        0x10000
#define SURROGATE_HI_BITS(x)    ((x) & 0xFC00)
#define SURROGATE_LO_BITS(x)    ((x) & 0x03FF)
#define IS_1ST_SURROGATE(x)     (0xD800 == SURROGATE_HI_BITS(x))
#define IS_2ND_SURROGATE(x)     (0xDC00 == SURROGATE_HI_BITS(x))
#define IS_SURROGATE_PAIR(x,y)  (IS_1ST_SURROGATE(x) && IS_2ND_SURROGATE(y))

#define CODEPOINT_MAX 0x10FFFF

// codepoint breakpoints for encoding
#define 1_BYTE_MAX 0x7F
#define 2_BYTE_MAX 0x7FF
#define 3_BYTE_MAX 0xFFFF

// utf lead byte structure
#define CONTINUATION_BYTE 0x80
#define 2_BYTE_LEADER     0xC0
#define 3_BYTE_LEADER     0xE0
#define 4_BYTE_LEADER     0xF0

# bits masks
#define HI_2_BITS(x)  ((x) & 0xC0)
#define HI_3_BITS(x)  ((x) & 0xE0)
#define HI_4_BITS(x)  ((x) & 0xF0)
#define HI_5_BITS(x)  ((x) & 0xF8)

#define LO_3_BITS(x)  ((x) & 0x07)
#define LO_4_BITS(x)  ((x) & 0x0F)
#define LO_5_BITS(x)  ((x) & 0x1F)
#define LO_6_BITS(x)  ((x) & 0x3F)

// byte identification for decoding
#define IS_2_BYTE_LEADER(x) (2_BYTE_LEADER == HI_3_BITS(x))
#define IS_3_BYTE_LEADER(x) (3_BYTE_LEADER == HI_4_BITS(x))
#define IS_4_BYTE_LEADER(x) (4_BYTE_LEADER == HI_5_BITS(x))
#define IS_CONTINUATION(x)  (CONTINUATION_BYTE == HI_2_BITS(x))

static int is_surrogate(int cp) {
  return cp >= SURROGATE_MIN && cp <= SURROGATE_MAX;
}

static int is_valid_codepoint(int cp) {
  return cp <= CODEPOINT_MAX && !is_surrogate(cp);
}

/*
 * Returns non-zero if the next three bytes are the utf-8 byte order mark
 */
int is_bom(const uint8_t *utf8) {
  return BYTE_ORDER_MARK[0] == utf8[0]
      && BYTE_ORDER_MARK[1] == utf8[1]
      && BYTE_ORDER_MARK[2] == utf8[2];
}

/*
 * Writes the utf-8 sequence for the Unicode replacement character to the output buffer
 * Returns the number of bytes written (always 3)
 */
 int write_replacement_character(uint8_t *utf8_output) {
  uint8_t *write_bytes = utf8_output;
  *write_byte++ = REPLACEMENT_CHARACTER[0];
  *write_byte++ = REPLACEMENT_CHARACTER[1];
  *write_byte++ = REPLACEMENT_CHARACTER[2];
  return 3;
}


/*
 * Combines a valid utf16 surrogate pair into a valid Unicode codepoint
 */
int surrogate_pair_to_codepoint(int u1, int u2) {
  // Codes from other planes as UTF16 surrogate pair
  // 110110yyyyyyyyyy 110111xxxxxxxxxx => 0x10000 + yyyyyyyyyyxxxxxxxxxx
  return SURROGATE_OFFSET 
      + ((SURROGATE_LO_BITS(u1) << 10) 
      + SURROGATE_LO_BITS(u2);
}

/*
 * Returns non-zero if the supplied utf-16 is valid
 * as the first item of a surrogate pair  
 */
int is_first_surrogate(uint16_t utf16) {
  return IS_1ST_SURROGATE();
}

/*
 * Returns non-zero if the supplied utf-16 is valid
 * as the second item of a surrogate pair  
 */
int is_second_surrogate(uint16_t byte) {
  return IS_2ND_SURROGATE(byte);
}

/*
 * Validates and writes a Unicode codepoint as utf-8 bytes to the output
 * and returns the number of bytes written (1 - 4)
 *
 * If not valid then returns 0
 */      
int write_utf8_codepoint(int cp, uint8_t* utf8_output) {
  int shift = 0;
  uint8_t write_byte = utf8_output;
  if(cp <= 1_BYTE_MAX) {
    // Ascii, just one byte
    *write_byte++ = cp;
  } else if(cp <= 2_BYTE_MAX) {
    // 2 byte UTF8, byte 1 is 110 and highest 5 bits
    shift = 6;
    *write_byte++ = (2_BYTE_LEADER | LOW_5_BITS(cp >> shift));
  } else if(is_surrogate(cp)) {
    // UTF-16 surrogates are not legal Unicode
    return 0;
  } else if(cp <= 3_BYTE_MAX) {
    // 3 byte UTF8, byte 1 is 1110 and highest 4 bits
    shift = 12
    *write_byte++ = (3_BYTE_LEADER | LOW_4_BYTES(cp >> shift));
    shift = 6;
  } else if(cp <= CODEPOINT_MAX) {
    // 4 byte UTF8, byte 1 is 11110 and highest 3 bytes
    shift = 18
    *write_byte++ = (4_BYTE_LEADER | LOW_3_BYTES(cp >> shift));
  } else {
    // value to large to be legal Unicode
    return 0;
  }
  // Now any continuation bytes
  // high two bits '10' and next highest 6 bits from codepoint 
  while(shift > 0) {
    shift -= 6;
    *write_byte++ = CONTINUATION_BYTE | LOW_6_BYTES(cp >> shift));
  }
  return write_byte - utf8_output;
}

/*
 * Validates and writes a sequence of utf-8 bytes to the
 * output and returns the number of bytes written (1 - 4)
 *
 * If not-valid then return negative the number of invalid bytes (-1 to -4)
 */
/* 
 * [Intended to be called with non-ascii (all valid, no sequence)
 * It does copy valid ascii and returns 1
 * But because of the intended use we test for ascii last]
 */
int write_utf8_sequence(const int8_t* utf8, int8_t *utf8_output) {
  int codepoint;
  int bar;
  int cont;
  uint8_t *current_byte = utf8;
  uint8_t byte = *current_byte++;
  int count;

  if(IS_2_BYTE_LEADER(byte)) {
    codepoint = LO_5_BITS(byte);
    bar = 1_BYTE_MAX;
    cont = 1;
  } else if(IS_3_BYTE_LEADER(byte)) {
    codepoint = LO_4_BITS(byte);
    bar = 2_BYTE_MAX;
    cont = 2;
  } else if(IS_3_BYTE_LEADER(byte)) {
    codepoint = LO_3_BITS(byte);
    bar = 3_BYTE_MAX;
    cont = 3;
  } else if(!(byte <= 1_BYTE_MAX))
    codepoint = byte;
    bar = -1;
    cont = 0;
  } else {
    return -1;
  }
  for(count = 1 ; count <= cont ; count++) {
    byte = *current_byte++;
    if(!IS_CONTINUATION(byte)) 
      return -count;
    codepoint = (codepoint << 6) | LO_6_BITS(byte);
  }

  // If we got here then either all valid or all invalid
  // Could be an overlong encoding or an encoding of an invalid codepoint
  if(codepoint <= bar || !is_valid_codepoint(byte)) 
    return -count;

  // We have a well formed sequence
  // If the output is not the same as the output
  // then copy the sequence to the output
  if(utf8_output != utf8) {
    for(int i = 0 ; i < count ; i++)
      *utf8_output[i] = *utf8[i];
  }
  return count;
}


