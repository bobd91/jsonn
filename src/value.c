
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "jsonn.h"
#include "parse.h"

#include "config.c"
#include "utf8.c"
#include "error.c"

#define STRING_SPECIALS_DOUBLE  "\\\""
#define STRING_SPECIALS_SINGLE  "\\'"
#define STRING_SPECIALS_KEY     "\\ :"
#define STRING_SPECIALS_STRING  "\\ ,]}"

static void consume_line_comment(jsonn_parser p) {
        while(p->current < p->last
                        && '\r' != *p->current 
                        && '\n' != *p->current)
                p->current++;
}

static void consume_block_comment(jsonn_parser p) {
        while(1) {
                while(p->current < p->last 
                                && '*' != *p->current)
                        p->current++;
                if('/' == *p->current) 
                        return;
        }
}

static void consume_whitespace(jsonn_parser p) {
        while(p->current < p->last) {
                switch(*p->current) {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                        break;
                case '/':
                        if(!(p->flags & FLAG_COMMENTS)) return;

                        char next_byte = *(1 + p->current);
                        switch(next_byte) {
                        case '/':
                                p->current += 2;
                                consume_line_comment(p);
                                break;
                        case '*':
                                p->current += 2;
                                consume_block_comment(p);
                                break;
                        default:
                                return;
                        }
                default:
                        return;
                }
                p->current++;
        }
}

static int skip_sequence(jsonn_parser p, uint8_t *sequence) {
        uint8_t *s = p->current;
        while(*sequence && *s++ == *sequence++)
                ;

        if(*sequence) return 0;

        p->current = s;
        return 1;
}

// Parse four hex digits [0-9, a-f, A-F]
// Returns the integer value or -1 on invalid input
static int parse_4hexdig(jsonn_parser p) {
        int value = 0;
        uint8_t *value_byte = p->current;
        for(int i = 0 ; i < 4 ; i++) {
                int v = *value_byte++;
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
        p->current = value_byte;
        return value;
}

// Parse XXXX of \uXXXX sequence into a Unicode codepoint
//
// If it finds a UTF-16 first surrogate then look ahead for a \uXXXX 
// that is a second surrogate and convert the two surrogates into a codepoint
//
// Returns -1 on invalid input
static int parse_unicode_escapes(jsonn_parser p) {
        int u = parse_4hexdig(p);
        if(u < 0) 
                return -1;

        if(is_first_surrogate(u)) {
                uint8_t *m = p->current;
                int u2 = skip_sequence(p, (uint8_t *)"\\u") ? parse_4hexdig(p) : -1;
                if(is_second_surrogate(u2)) {
                        u = surrogate_pair_to_codepoint(u, u2);
                } else {
                        // This is not the surrogate you are looking for ...
                        p->current = m;
                }
        }
        return u;
}


static jsonn_type set_long_result(jsonn_parser p, long long_value) 
{
        p->result.is.number.long_value = long_value;
        return JSONN_LONG;
}

static jsonn_type set_double_result(jsonn_parser p, double double_value) 
{
        p->result.is.number.double_value = double_value;
        return JSONN_DOUBLE;
}

static jsonn_type set_string_result(
                jsonn_parser p,
                uint8_t *string, 
                int length, 
                jsonn_type type) 
{
        p->result.is.string.bytes = string;
        p->result.is.string.length = length;
        return type;
}

static uint8_t next_special(jsonn_parser p, char *specials) 
{
        uint8_t byte;
        if(p->current == p->write) {
                // nothing moved so we can just skip bytes
                while(p->current < p->last) {
                        byte = *p->current;
                        if(strchr(specials, (int)byte)
                                        || 0x20 > byte
                                        || 0x7F < byte) 
                                break;
                        p->current++;
                }
                p->write = p->current;
        } else {
                // things have moved so we have to copy bytes
                while(p->current < p->last) {
                        byte = *p->current;
                        if(strchr(specials, (int)byte)
                                        || 0x20 > byte
                                        || 0x7F < byte) 
                                break;
                        *p->write++ = *p->current++;
                }
        }
        return *p->current;
}

/*
 * Parse JSON string into utf-8
 *
 * Returns JSONN_STRING or JSONN_KEY if the string was converted to utf8
 *         JSONN_UNEXPECTED if the string could not be converted 
 * The actual string is set in jsonn_result
 *
 * The returned string is in the same place as it is in the JSON string
 * except that it is NULL terminated (usually overwriting the closing "s)
 *
 * All leading unescaped bytes can be skipped
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
 * the original JSON string.  If situation arrises then no replacement
 * will be made and JSONN_UNEXPECTED will be returned.
 */
static jsonn_type parse_string(jsonn_parser p, jsonn_type type) 
{
        char *specials = NULL;
        uint8_t quoted = '\0';

        switch(*p->current) {
        case '"':
                specials = STRING_SPECIALS_DOUBLE;
                quoted = '"';
                p->current++;
                break;

        case '\'':
                specials = STRING_SPECIALS_SINGLE;
                quoted = '\'';
                p->current++;
                break;

        default:
                switch(type) {
                case JSONN_STRING:
                        if(p->flags & FLAG_UNQUOTED_STRINGS)
                                specials = STRING_SPECIALS_STRING;
                        break;

                case JSONN_KEY:
                        if(p->flags & FLAG_UNQUOTED_KEYS)
                                specials = STRING_SPECIALS_KEY;
                        break;

                default:
                        break;
                }
        }

        if(!specials)
                return parse_error(p);

        int illformed_utf8 = 0;
        p->terminator = '\0';
        uint8_t *start = p->current;

        while(p->current < p->last) {

                // Next byte will be quote terminator, ascii control, UTF-8 
                // or NULL if we hit EOF
                uint8_t byte = next_special(p, specials);
                switch(byte) {
                case ',':
                case '}':
                case ']':
                case ':':
                        // terminating unquoted string with \0
                        // will overwrite this byte
                        // so store it for next parse
                        p->terminator = byte;
                // fall through
                case '"':
                case '\'':
                case ' ':
                        p->current++;
                        *p->write = '\0';
                        return set_string_result(p, start, p->write - start, type);

                case '\\':
                        byte = *p->current++;

                        // if string not quoted then just escape char
                        if(!quoted) {
                                *p->write++ = byte;
                                break;
                        }

                        switch(byte) {
                        case '"':
                        case '\\':
                        case '/':
                                *p->write++ = byte;
                                break;

                        case 'b':
                                *p->write++ = '\b';
                                break;

                        case 'f':
                                *p->write++ = '\f';
                                break;

                        case 'n':
                                *p->write++ = '\n';
                                break;

                        case 'r':
                                *p->write++ = '\r';
                                break;

                        case 't':
                                *p->write++ = '\t';
                                break;

                        case 'u':
                                // \uXXXX [\uXXXX]
                                int cp = parse_unicode_escapes(p);
                                if(cp < 0)
                                        return parse_error(p);
                                illformed_utf8 = !write_utf8_codepoint(p, cp);
                                break;

                        default:
                                if(p->flags & FLAG_ESCAPE_CHARACTERS)
                                        *p->write++ = byte;
                                else
                                        return parse_error(p);
                                break;
                        }

                default:
                        if(0x20 > byte) {
                                return parse_error(p);
                        } else {
                                illformed_utf8 = !write_utf8_sequence(p);
                        }
                }

                if(illformed_utf8) {
                        // replace illformed utf8 if configured and there is room for it
                        if(p->flags & FLAG_REPLACE_ILLFORMED_UTF8
                                        && p->current - p->write > replacement_length) {
                                write_replacement_character(p);
                                illformed_utf8 = 0;
                        } else {
                                error(p, JSONN_ERROR_ILLFORMED_UTF8);
                        }
                }
        }
        return JSONN_EOF;
}

/*
 * Calls parse_string to parse the key
 * If successful return JSONN_KEY, else JSONN_UNEXPECTED
 */
static jsonn_type parse_key(jsonn_parser p) 
{
        return parse_string(p, JSONN_KEY);
}

/*
 * Parse JSON number as double or long
 * Return JSONN_DOUBLE or JSONN_LONG and set value into jsonn_result
 * Return JSONN_UNEXPECTED if string cannot be parsed as a number
 *
 * JSON doesn't permit leading 0s or hex numbers but strtod does
 * so validate first.
 *
 * If double value can be convertted to long without loss 
 * and the input text does not contain a decimal point
 * then return the long value
 */
static jsonn_type parse_number(jsonn_parser p) 
{
        uint8_t *start = p->current;
        uint8_t *double_end;
        double double_val;
        int64_t long_val;

        if('-' == *p->current)
                p->current++;

        // Leading 0 should not be followed by a digit or x or X
        if('0' == *p->current && strchr("01234567890xX", *(p->current + 1)))
                return parse_error(p);

        errno = 0;
        double_val = strtod((char *)start, (char **)&double_end);
        if(errno) return parse_error(p);

        p->current = double_end;
        long_val = (int64_t)double_val;
        if(double_val == long_val 
                        && !memchr(start, '.', double_end - start)) 
                return set_long_result(p, long_val);
        else
                return set_double_result(p, double_val);
}

