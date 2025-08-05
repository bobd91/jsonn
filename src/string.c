#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define STRING_SPECIALS_DOUBLE  "\\\""
#define STRING_SPECIALS_SINGLE  "\\'"
#define STRING_SPECIALS_KEY     "\\ :"
#define STRING_SPECIALS_STRING  "\\ ,]}"

// used to skip second \u when parsing \uXXXX\uXXXX
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
        for(int i = 0 ; i < 4 ; i++) {
                int v = *p->current++;
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

        if(is_surrogate(u)) {
                if(!is_first_surrogate(u))
                        return -1;
                
                int u2 = skip_sequence(p, (uint8_t *)"\\u") 
                        ? parse_4hexdig(p) 
                        : -1;

                if(!is_second_surrogate(u2))
                        return -1;

                u = surrogate_pair_to_codepoint(u, u2);
        }
        return u;
}

static jsonn_type set_string_result(
                jsonn_parser p,
                uint8_t *start, 
                int complete,
                jsonn_type type) 
{
        p->result.string.bytes = start;
        p->result.string.length = p->write - start;
        p->result.string.complete = complete;
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

// Set p->repeat_next so that parse_next() will carry on with the
// string that we are in the middle of
static jsonn_type string_continues(jsonn_parser p, uint8_t *start, jsonn_type type)
{
        if(type == JSONN_KEY)
                p->repeat_next = PARSE_KEY_NEXT;
        else
                p->repeat_next = PARSE_STRING_NEXT;

        return set_string_result(p, start, 0, type);
}

/*
 * Parse JSON string into utf-8
 *
 * Returns <type> argument if the string was converted to utf8
 *         JSONN_ERROR if the string could not be converted 
 * The actual string is set in jsonn_result
 *
 * The returned string is in the same place as it is in the JSON string
 *
 * All leading unescaped bytes can be skipped
 * Escape sequences are translated to actual bytes, which will be
 * shorter than the JSON representation.  
 * After an escape sequence bytes will be copied rather than skipped.
 *
 * If an escape sequence may cross into the following block then
 * the sequence is copied to the front of the next block
 *
 * Any non-ascii utf-8 sequences are checked to see if they are well formed.
 * If illformed JSONN_ERROR will be returned.
 */
static jsonn_type parse_string(jsonn_parser p, jsonn_type type) 
{
        char *specials = NULL;
        uint8_t quote = '\0';

        switch(*p->current) {
        case '"':
                specials = STRING_SPECIALS_DOUBLE;
                quote = '"';
                p->current++;
                break;

        case '\'':
                if(p->flags & JSONN_FLAG_SINGLE_QUOTES) {
                        specials = STRING_SPECIALS_SINGLE;
                        quote = '\'';
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

        uint8_t *start = ensure_current(p);
        if(!start)
                return alloc_error(p);

        p->write = p->current;

        // String parsing stays with a single block
        // and if end of block is encountered within
        // a string then then a partial string is returned
        while(p->current < p->last) {

                // Next byte will be quote terminator, ascii control, UTF-8 
                // or NULL if we hit end of block or EOF
                uint8_t byte = next_special(p, specials);
                switch(byte) {
                case ',':
                case '}':
                case ']':
                case ':':
                case '"':
                case '\'':
                case ' ':
                        p->current++;
                        return set_string_result(p, start, 1, type);

                case '\\':
                        // Need to lookahead as escape sequence could
                        // cross block boundary
                        // Simple escape sequences are 2 bytes
                        // but unicode escapes are 12 bytes
                        if(!ensure_string(p, 2, quote)
                                        || ('u' == *(p->current + 1)
                                                && !ensure_string(p, 12, quote))) {
                                // had to copy escape sequence to next block
                                // return incomplete string
                                // and set up to continue string parse next time
                                return string_continues(p, start, type);
                        }

                        // we have entire escape sequence
                        p->current++; // skip the \ without writing

                        byte = *p->current++;

                        // if string not quoted then just escape char
                        if(!quote) {
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
                                if(!write_utf8_codepoint(p, cp))
                                        return utf8_error(p);
                                break;

                        default:
                                if(p->flags & JSONN_FLAG_ESCAPE_CHARACTERS)
                                        *p->write++ = byte;
                                else
                                        return parse_error(p);
                                break;
                        }
                        break;

                case '\0':
                        // Is this NULL byte in string or end of block/file
                        if(p->current == p->last)
                                break;
                        // fallthrough
                default:
                        if(0x20 > byte) {
                                return parse_error(p);
                        } else {
                                if(!write_utf8_sequence(p))
                                        return utf8_error(p);
                        }
                }
        }

        if(p->seen_eof) {
                // eof in string permitted if not quoted
                return (!quote)
                        ? set_string_result(p, start, 1, type)
                        : parse_error(p);
        }

        // Make sure next buffer starts with a quote
        if(!copy_prefix(p, quote))
                return alloc_error(p);

        return string_continues(p, start, type); 
}

/*
 * Calls parse_string to parse the key
 * If successful return JSONN_KEY, else JSONN_ERROR
 */
static jsonn_type parse_key(jsonn_parser p) 
{
        return parse_string(p, JSONN_KEY);
}
