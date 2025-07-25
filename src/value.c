#include <stdint.h>

#include "jsonn.h"

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
                        p->current;
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
                        if(!p->flags & FLAG_ALLOW_COMMENST) return;

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
        return 1
}

static void write_byte(jsonn_parser p, uint8_t byte) {
        *p->write++ = byte;
        p->current++;
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
                int u2 = skip_sequence("\\u") ? parse_4hexdig(p) : -1;
                if(is_second_surrogate(u2)) {
                        u = surrogate_pair_to_codepoint(u, u2);
                } else {
                        // This is not the surrogate you are looking for ...
                        current_byte = m;
                }
        }
        return u;
}

// Parse four hex digits [0-9, a-f, A-F]
// Returns the integer value or -1 on invalid input
static int parse_4hexdig(jsonn_parser p) {
        int value = 0;
        char *value_byte = p->current;
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

static jsonn_type long_result(long long_value, jsonn_result *result) 
{
        result->value.is.long_value = long_value;
        return JSONN_LONG;
}

static jsonn_type double_result(double double_value, jsonn_result *result) 
{
        result->value.is.double_value = double_value;
        return JSONN_DOUBLE;
}

static jsonn_type string_result(
                utf8_t *string, 
                int length, 
                jsonn_result *result) 
{
        result->value.is.string.bytes = string_value;
        result->value.is.string.length = length;
        return JSONN_STRING;
}

static uint8_t next_special_byte(jsonn_parser p) 
{
        if(p->current == p->write) {
                while(0x20 < *p->current
                                && '"' != *p->current
                                && '\\' != *p->current
                                && 0x80 > *p->current) {
                        if(p->current == p->last) { 
                                p->write = p->current;
                                break;
                        }
                        p->current++;
                }
                p->write = p->current;
        } else {
                while(0x20 < *p->current
                                && '"' != *p->current
                                && '\\' != *p->current
                                && 0x80 > *p->current) {
                        if(p->current == p->last)
                                break;
                        *p->write++ = *p->current++;
                }
        }
        return *p->current;
}




/*
 * Same as parse_string except on success it returns JSONN_NAME
 */
jsonn_type parse_name(jsonn_parser p, jsonn_value *result) 
{
        jsonn_type type = parse_string(p, result);
        return (type == JSONN_STRING) 
                ? JSONN_NAME
                : JSONN_UNEXPECTED;
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
static jsonn_type parse_string(jsonn_parser p, jsonn_result *result) 
{
        int illformed_utf8 = 0;

        if('"' != *p->current)
                return JSONN_UNEXPECTED;

        uint8_t *start = ++p->current;

        while(p->current < p->last) {

                // Next byte will be ", \, ascii control, UTF-8 or NULL if we hit EOF
                uint8_t byte = next_special_byte(p);
                if(!byte)
                        break; 

                if('"' == byte) {
                        p->current++;
                        *p->write = '\0';
                        return string_result(start, p->write - start, result);
                } else if('\\' == byte) {
                        p->current++;
                        switch(*p->current) {
                        case '"':
                        case '\\':
                        case '/':
                                write_byte(*p->current);
                                break;

                        case 'b':
                                write_byte('\b');
                                break;

                        case 'f':
                                write_byte('\f');
                                break;

                        case 'n':
                                write_byte('\n');
                                break;

                        case 'r':
                                write_byte('\r');
                                break;

                        case 't':
                                write_byte('\t');
                                break;

                        case 'u':
                                // \uXXXX [\uXXXX]
                                p->current++;
                                int cp = parse_unicode_escapes(p);
                                if(cp < 0)
                                        return JSONN_UNEXPECTED;
                                illformed_utf8 = !write_utf8_codepoint(p, cp);
                                break;

                        default:
                                return JSONN_UNEXPECTED;
                        }

                        if(replace) {
                                p->write++ = replace;
                                p->current++;
                        }

                } else if(0x20 > *p->current) {
                        return JSONN_UNEXPECTED;

                } else {
                        // UTF-8
                        illformed_utf8 = !write_utf8_sequence(p);
                }

                if(illformed_utf8) {
                        // replace illformed utf8 if configured and there is room for it
                        if(p->flags & FLAG_REPLACE_ILLFORMED_UTF8
                                        && p->current - p->write > replacement_length) {
                                write_replacement(p);
                                illformed_utf8 = 0;
                        } else {
                                return JSONN_UNEXPECTED;
                        }
                }
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
 * If double value can be convertted to long without loss 
 * and the input text does not contain a decimal point
 * then return the long value
 */
jsonn_type parse_number(jsonn_parser p, jsonn_result *result) 
{
        uint8_t *start = p->current;
        uint8_t byte = *p->current;
        uint8_t *double_end;
        double double_val;
        int64_t long_val;

        if('-' == *)
                byte++;

        // Leading 0 should not be followed by a digit or x or X
        if('0' == *byte && strchr('01234567890xX', *(byte + 1)))
                return JSONN_UNEXPECTED;

        errno = 0;
        double_val = strtod(start, &double_end);
        if(errno) return JSONN_UNEXPECTED;

        p->current = double_end;
        long_val = (int64_t)double_val;
        if(double_val == long_val 
                        && !strnchr(start, double_end - start, '.')) 
                return long_result(long_val, result);
        else
                return double_result(double_val, result);
}

