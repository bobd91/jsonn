
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "jsonn.h"
#include "parse.h"

#include "error.c"
#include "config.c"
#include "utf8.c"

#define STRING_SPECIALS_DOUBLE  "\\\""
#define STRING_SPECIALS_SINGLE  "\\'"
#define STRING_SPECIALS_KEY     "\\ :"
#define STRING_SPECIALS_STRING  "\\ ,]}"


static int skip_sequence(jsonn_parser p, uint8_t *sequence) {
        // parse_string will have ensured that
        // if we find the sequence it will be in this buffer
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
        // parse_string will have ensured that
        // if we find the XXXX it will be in this buffer
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
        // parse_string will have ensured that
        // entire \uXXXX\uXXXX will be in this buffer

        int u = parse_4hexdig(p);
        if(u < 0) 
                return -1;

        if(is_surrogate(u)) {
                if(is_first_surrogate(u)) {
                        int u2 = skip_sequence(p, (uint8_t *)"\\u")
                                ? parse_4hexdig(p)
                                : -1;
                        if(is_second_surrogate(u2)) {
                                u = surrogate_pair_to_codepoint(u, u2);
                        } else {
                                // 1st not followed by second is illformed
                                return -1;
                        }
                } else {
                        // surrogate but not a valid 1st in pair
                        return -1;
                }
        }
        return u;
}


static jsonn_type set_integer_result(jsonn_parser p, long integer) 
{
        p->result.number.integer = integer;
        return JSONN_INTEGER;
}

static jsonn_type set_real_result(jsonn_parser p, double real) 
{
        p->result.number.real = real;
        return JSONN_REAL;
}

// TODO convert to buffers, returning partial strings
static jsonn_type set_string_result(
                jsonn_parser p,
                uint8_t *string, 
                int length, 
                int complete,
                jsonn_type type) 
{
        p->result.string.bytes = string;
        p->result.string.length = length;
        p->result.string.complete = complete;
        return type;
}

// string parsing, all string results must be in same block

static uint8_t next_special(jsonn_parser p, char *specials) 
{
        // will return '\0' if it reaches the end of the block
        while(p->current < p->last) {
                uint8_t byte = *p->current;
                if(strchr(specials, (int)byte)
                                || 0x20 > byte
                                || 0x7F < byte) 
                        break;
                p->current++;
        }
        return *p->current;
}

// Do we have string sequence of given length
// If not we copy remaining bytes into next block
// Preceeded by a quote if required
// Return 1 if we have sufficient bytes
int string_lookahead(jsonn_parser p, int length, uint8_t quote) {
        if(p->last - p->current >= length)
                return 1;
        // TODO
        // make new buffer
        // copy in quote (unless '\0')
        // copy in bytes
        // return 0
}

/*
 * Parse JSON string into utf-8
 *
 * Returns JSONN_STRING or JSONN_KEY if the string was converted to utf8
 *         JSONN_STRING_NEXT or JSONN_KEY_NEXT if string is a continuation
 *         of the previous string
 *         JSONN_ERROR if the string could not be converted 
 *
 * The actual string available via jsonn_result
 *
 * The returned string is in the same place as it is in the JSON string
 *
 * All unescaped bytes can be skipped
 *
 * Escape sequences are translated to actual bytes, which will be
 * shorter than the JSON representation.
 * Escaped results will be placed at the end of the string and the
 * partial string returned.
 * Unless the string is currently empty, in which case they will be placed 
 * immediately before the next part of the buffer to be parsed.
 *
 * TODO buffers, can we write over \ then return partial
 *      which will allow no writing
 *      will be harder to do with \uXXXX across buffer boundaries
 *      but should be possible
 *      No write management would be a win
 *
 * Any non-ascii utf-8 sequences are checked to see if they are well formed.
 * Parse will fail and JSONN_ERROR returned on any illformed sequence
 */
// TODO: buffers, escaping, writing, partials
//
// Problems with restarting string parse:
//  - quote symbol -> copy across quote
//  - previous \uXXXX if first surrogate -> copy across 12 on \u
//
static jsonn_type parse_string(jsonn_parser p, jsonn_type type, int continuation) 
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
                if(p->flags & JSONN_FLAG_SINGLE_QUOTES) {
                        specials = STRING_SPECIALS_SINGLE;
                        quoted = '\'';
                        p->current++;
                        break;
                }
                // fall through
        default:
                switch(type) {
                case JSONN_STRING:
                        if(p->flags & JSONN_FLAG_UNQUOTED_STRINGS)
                                specials = STRING_SPECIALS_STRING;
                        break;

                case JSONN_KEY:
                        if(p->flags & JSONN_FLAG_UNQUOTED_KEYS)
                                specials = STRING_SPECIALS_KEY;
                        break;

                default:
                        break;
                }
        }

        if(!specials)
                return parse_error(p);

        int illformed_utf8 = 0;

        // If opening " was at the end of a block
        // then need to make sure that we start at a new block
        uint8_t *start = ensure_current(p);
        if(!start)
                return alloc_error(p);

        // But then we stay within the block
        // Any cross block work has to be done on a case by case basis
        while(p->current < p->last) {

                // Next byte will be quote terminator, ascii control, UTF-8 
                // or NULL if we hit end of block
                uint8_t byte = next_special(p, specials);

                // If at end of string then return the string
                switch(byte) {
                case ',':
                case '}':
                case ']':
                case ':':
                case '"':
                case '\'':
                case ' ':
                        res = string_complete(p, start, type);
                        p->current++; // TODO should this be in string_...
                        return res;

                case '\\':
                        // requires lookahead
                        // 2 bytes for normal escapes but 12 for \u escapes
                        if(string_lookahead(2)
                                && (('u' != *(p->current + 1))
                                        || string_lookahead(12)))
                                process_escape(p);
                        else
                                // TODO make sure we start at correct place next time
                                return string_partial(p, start, type);
                        break;

                case '\0':
                        // Is this NULL byte in string or end of buffer
                        if(p->current == p->last) {
                                ensure_current(p);
                                break;
                        }
                        // fallthrough
                default:
                        if(0x20 > byte) {
                                return parse_error(p);
                        } else {
                                // requires lookahead
                                if(string_lookahead(4))
                                        if(!is_valid_utf8_sequence(p))
                                                return utf8_error(p);
                                else
                                        // TODO make sure we start at correct place next time
                                        return string_partial(p, start, type);

                        }
                }
        }

        // Not found the end of string, reject if require end quote
        if(quoted)
                return parse_error(p);

        return string_complete(p, start, type);

}

                // if we are not in the last block then we have to be wary of
                // trying to work with sequences of input that cross boundaries
                // the easiest way to deal with this is to stop short of the end
                // of the block and copy over the remaining characters to the new
                // block.  We have to do this in certain circumstances anyway
                // so rather than detect and code round each in turn we take
                // a less efficient but simpler route, always copy a sequence
                // that could be too short.
                // The longest sequence that can cause a problem is the 
                // surrogate pair escape sequence \uXXXX\uXXXX, 12 bytes
                // So if less than 12 bytes in the block:
                //  - create a new block
                //  - if we have bytes in the string to be processed
                //    - add a quote at the front of the new block
                //    - copy the unproceesed bytes to the block
                //    - read new bytes to the block
                //    - return the previous bytes as a partial string
                //  - if no bytes to be processed
                //    - copy the unproceesed bytes to the block
                //    - read new bytes to the block
                //    - process the bytes in the new block

                int remaining = p->last - p->current;
                if((!p->seen_eof) && remaining < 12) {
                        if(p->current - start) {
                                res = string_result(p, start, 0, type);

                                if(!ensure_string(p, quote, remaining))
                                        return alloc_error;

                                return res;
                        } else {
                                start = ensure_string(p, '\0', remaining);
                                if(!start)
                                        return alloc_error(p);

                        }
                }

                int have_bytes = start != p->current;

                switch(byte) {
                case '\\':
                        uint8_t *esc_start = p->current++;
                        byte = *p->current; 

                        // only have special escapes if string is quoted
                        if(!quoted) {
                                escape = byte;
                        } else {
                                switch(byte) {
                                case '"':
                                case '\\':
                                case '/':
                                        escape = byte;
                                        break;

                                case 'b':
                                        escape = '\b';
                                        break;

                                case 'f':
                                        escape = '\f';
                                        break;

                                case 'n':
                                        escape = '\n';
                                        break;

                                case 'r':
                                        escape = '\r';
                                        break;

                                case 't':
                                        escape = '\t';
                                        break;

                                case 'u':
                                        // \uXXXX [\uXXXX]
                                        codepoint = parse_unicode_escapes(p);
                                        if(cp < 0)
                                                return parse_error(p);
                                        break;

                                default:
                                        if(!p->flags & JSONN_FLAG_ESCAPE_CHARACTERS)
                                                return parse_error(p);
                                        escape = byte;
                                        break;
                                }
                        }
                        // Escape sequence means we are out of step with string
                        // in buffer so
                        //   - add our escape results at end of string
                        //   - if we have quote add it to start of next string
                        //   - reset current to quote or start of next string
                        //   - return partial string
                        utf8_t *next = p->current;
                        p->current = esc_start;
                        if(codepoint >= 0) {
                                utf8_len = write_utf8_codepoint(p, codepoint);
                                if(!utf8_len)
                                        return utf8_error(p);
                        } else {
                                *p->current++ = escape;
                        }
                        if(quote) {
                                next--;
                                *next = quote;
                        }
                        res = string_result(p, start, 0, type);
                        p->current = next;
                        return res;
                }
                break;


                if(illformed_utf8) {
                        return error(p, JSONN_ERROR_UTF8);
                }
        }
        // eof in string is end of string if not quoted


/*
 * Calls parse_string to parse the key
 * If successful return JSONN_KEY, else JSONN_ERROR
 */
static jsonn_type parse_key(jsonn_parser p) 
{
        return parse_string(p, JSONN_KEY);
}

// Increment pos whilst parsing a number
// If we run into end of buffer then 
// allocate new buffer, copy across all seen bytes
// and setup to continue parsing at the same position
static uint8_t *next_number_pos(jsonn_parser, uint8_t *pos)
{
        if(p->last > ++pos)
                return pos;
        int copy_count = p->last - p->current;
        if(ensure_current_n(p, copy_count))
                return p->current + copy_count;
        // failed to allocate a new block
        // returning a pointer to last (which is pointer to null byte)
        // is safe as parse number will not match anything
        return pos;


/*
 * Parse JSON number as real or integer
 * Return JSONN_REAL or JSONN_INTEGER and set value into jsonn_result
 * Return JSONN_ERROR if string cannot be parsed as a number
 *
 * JSON is much tighter about what numbers it allows than the library functions
 * strtod or strtol that we use to convert text to numbers 
 * so we have to do a lot of sanity checking first.
 *
 * TODO: error reporting, where is <at> after failed number parse
 */
static jsonn_type parse_number(jsonn_parser p) 
{
        uint8_t number_buf[MAX_NUMBER_LENGTH + 1];
        uint8_t *pos = ensure_current(p);
        uint8_t *end;
        long integer;
        long double real;
        int has_minus = 0;
        int has_zero;
        int int_digits;
        int has_decimal = 0;
        int frac_digits = 0;
        int has_exp = 0;
        int has_exp_sign = 0;
        int exp_digits = 0;
        
        // sanity check
        if('-' == *p->current) {
                has_minus = 1;
                pos = next_number_pos(p, pos);
        }

        has_zero = ('0' == *pos);
        
        while(*pos >= '0' &&  *pos <= '9')
                pos = next_number_pos(p, pos);

        int_digits = pos - p->current - has_minus;

        if('.' == *pos) {
                has_decimal = 1;
                pos = next_number_pos(p, pos);

                while(*pos >='0' && *pos <= '9')
                        pos = next_number_pos(p, pos);

                frac_digits = pos
                        - p->current
                        - has_minus 
                        - int_digits
                        - 1;
        }

        if('e' == *pos || 'E' == *pos) {
                has_exp = 1;
                pos = next_number_pos(p, pos);
                if('-' == *pos || '+' == *pos) {
                        has_exp_sign = 1;
                        pos = next_number_pos(p, pos);
                }

                while(*pos >= '0' && *pos <= '9')
                        pos = next_number_pos(p, pos);

                exp_digits = pos
                        - p->current
                        - has_minus
                        - int_digits
                        - has_decimal
                        - frac_digits
                        - has_exp_sign
                        - 1;
        }
        
        if(int_digits == 0
                        || has_zero && int_digits > 1
                        || has_decimal && frac_digits == 0
                        || has_exp && exp_digits == 0) {
                return number_error(p);
        }

        if(has_decimal || has_exp) {
                errno = 0;
                real= strtold((char *)p->current, NULL);
                if(errno) return number_error(p);
                p->current = pos + 1;
                return set_real_result(p, real);
        } else {
                errno = 0;
                integer = strtol((char *)p->current, NULL, 10);
                if(errno) return number_error(p);
                p->current = pos + 1;
                return set_integer_result(p, integer);
        }
}

