
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jsonn.h"

#include "value.c"

static parse_next jsonn_init_next(jsonn_parser p)
{
        if(p->flags & FLAG_IS_OBJECT)
                return PARSE_OBJECT_MEMBER;
        else if(p->flags & FLAG_IS_ARRAY)
                return PARSE_ARRAY_VALUE;
        else
                return PARSE_VALUE;
}

static int pop_next(jsonn_parser p) 
{
        size_t sp = p->stack_pointer;
        if(sp == 0)
                return 0;
        p->stack_pointer--;
        p->next = 0x01 & p->stack[sp >> 3] >> (sp & 0x07);
        return 1;
}

static int push_next(jsonn_parser p, parse_next next) 
{
        size_t sp = p->stack_pointer;
        if(sp >= p->stack_size) 
                return 0;
        int offset = sp >> 3;
        int mask = 1 << (sp & 0x07);

        if(next)
                p->stack[offset] |= mask;
        else
                p->stack[offset] &= ~mask;

        p->stack_pointer++;
        p->next = next;
        return 1;
}

static uint8_t get_current_or_terminator(jsonn_parser p) 
{
        return p->terminator
                ? p->terminator
                : *p->current;
}

static void consume_current_or_terminator(jsonn_parser p)
{
        if(p->terminator)
                p->terminator = '\0';
        else
                p->current++;
}

static jsonn_type parse_begin_object(jsonn_parser p) 
{
        p->current++;
        return(push_next(p, PARSE_OBJECT_MEMBER_OPTIONAL))
                ? JSONN_BEGIN_OBJECT
                : error(p, JSONN_ERROR_STACKOVERFLOW);
}

static jsonn_type parse_end_object(jsonn_parser p) 
{
        consume_current_or_terminator(p);
        return(pop_next(p))
                ? JSONN_END_OBJECT
                : error(p, JSONN_ERROR_STACKUNDERFLOW);
}

static jsonn_type parse_begin_array(jsonn_parser p) 
{
        p->current++;
        return(push_next(p, PARSE_ARRAY_VALUE_OPTIONAL))
                ? JSONN_BEGIN_ARRAY
                : error(p, JSONN_ERROR_STACKOVERFLOW);
}

static jsonn_type parse_end_array(jsonn_parser p) 
{
        consume_current_or_terminator(p);
        return(pop_next(p))
                ? JSONN_END_ARRAY
                : error(p, JSONN_ERROR_STACKUNDERFLOW);
}

static jsonn_type parse_literal(
                jsonn_parser p, 
                char *literal, 
                jsonn_type type) 
{
        // Already matched first character of literal
        uint8_t *s1 = (uint8_t *)literal;
        uint8_t *s2 = p->current;
        while(*++s1 == *++s2 && *s1)
                ;
        // If we are at the null at the end of literal then we found it
        if(!*s1) {
                p->current = s2;
                return type;
        }
        return parse_error(p);
}

static jsonn_type parse_value(jsonn_parser p) 
{
        if(p->current < p->last) {
                switch(*p->current) {
                case '"':
                        return parse_string(p, 1);
                case '\'':
                        if(p->flags & FLAG_SINGLE_QUOTES)
                                return parse_string(p, 1);
                        break;

                case '{':
                        return parse_begin_object(p);
                case '[':
                        return parse_begin_array(p);
                case 'n':
                        return parse_literal(p, "null", JSONN_NULL);
                case 'f':
                        return parse_literal(p, "false", JSONN_FALSE);
                case 't':
                        return parse_literal(p, "true", JSONN_TRUE);
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
                        return parse_number(p);
                default:
                        if(p->flags & FLAG_UNQUOTED_STRINGS)
                                return parse_string(p, 0);
                        break;
                }
                return parse_error(p);
        }
        return JSONN_EOF;
}

static jsonn_type parse_array_terminator(jsonn_parser p) 
{
        if(p->current < p->last) {
                uint8_t byte = get_current_or_terminator(p);
                if(']' == byte) {
                        consume_current_or_terminator(p);
                        return parse_end_array(p);
                }
        }
        return parse_error(p);
}

static jsonn_type parse_object_terminator(jsonn_parser p) 
{
        if(p->current < p->last) {
                uint8_t byte = get_current_or_terminator(p);
                if('}' == byte) {
                        consume_current_or_terminator(p);
                        return parse_end_object(p);
                }
        }
        return parse_error(p);
}

static parse_next consume_array_value_separator(jsonn_parser p)
{
        if(p->current < p->last) {
                uint8_t byte = get_current_or_terminator(p);
                switch(byte) {
                case ',':
                        consume_current_or_terminator(p);
                        return (p->flags && FLAG_TRAILING_COMMAS)
                                ? PARSE_ARRAY_VALUE_OPTIONAL
                                : PARSE_ARRAY_VALUE;
                case ']':
                        // not separator so don't consume
                        return PARSE_ARRAY_TERMINATOR;

                default:
                        // not separator so don't consume
                        if(p->flags & FLAG_OPTIONAL_COMMAS)
                                return PARSE_ARRAY_VALUE;
                }
        }
        return (p->flags & FLAG_IS_ARRAY)
                ? PARSE_EOF
                : parse_error(p);
}

static parse_next consume_object_member_separator(jsonn_parser p) 
{
        if(p->current < p->last) {
                uint8_t byte = get_current_or_terminator(p);
                switch(byte) {
                case ',':
                        consume_current_or_terminator(p);
                        return (p->flags & FLAG_TRAILING_COMMAS)
                                ? PARSE_OBJECT_MEMBER_OPTIONAL
                                : PARSE_OBJECT_MEMBER;
                case '}':
                        // not separator so don't consume
                        return PARSE_OBJECT_TERMINATOR;

                default:
                        // not separator so don't consume
                        if(p->flags & FLAG_OPTIONAL_COMMAS)
                                return PARSE_OBJECT_MEMBER;
                }
        }
        return (p->flags & FLAG_IS_OBJECT)
                ? PARSE_EOF
                : parse_error(p);
}

static parse_next consume_object_name_separator(jsonn_parser p) 
{
        if(p->current < p->last) {
                uint8_t byte = get_current_or_terminator(p);
                if(':' == byte) {
                        consume_current_or_terminator(p);
                        return PARSE_OBJECT_MEMBER_VALUE;
                }
        }
        return parse_error(p);
}

static int is_valid(jsonn_type type) 
{
        return type != JSONN_ERROR;
}

static int is_value(jsonn_type type) 
{
        return type == JSONN_FALSE
                || type == JSONN_NULL
                || type == JSONN_TRUE
                || type == JSONN_LONG
                || type == JSONN_DOUBLE
                || type == JSONN_STRING;
}

static jsonn_type jsonn_next(jsonn_parser p)
{
        jsonn_type ret;
        while(1) {
                consume_whitespace(p);
                int optional = 1; // are values/pairs required?
                switch(p->next) {

                // bare value with no surrounding {} or []
                case PARSE_VALUE:
                        // {} [] will set next
                        // otherwise we should be at EOF
                        p->next = PARSE_EOF;
                        return parse_value(p);

                case PARSE_ARRAY_VALUE:
                        optional = 0;
                        // fall through
                case PARSE_ARRAY_VALUE_OPTIONAL:
                        ret = parse_value(p);
                        if(is_valid(ret)) {
                                if(is_value(ret))
                                        p->next = PARSE_ARRAY_VALUE_SEPARATOR;
                                return ret;
                        } else if (!optional) {
                                return ret;
                        }
                        p->next = PARSE_ARRAY_TERMINATOR;
                        break;

                case PARSE_ARRAY_VALUE_SEPARATOR:
                        p->next = consume_array_value_separator(p);
                        break;

                case PARSE_ARRAY_TERMINATOR:
                        return parse_array_terminator(p);

                case PARSE_OBJECT_MEMBER:
                        optional = 0;
                        // fall through
                case PARSE_OBJECT_MEMBER_OPTIONAL:
                        ret = parse_key(p);
                        if(is_valid(ret)) {
                                p->next = PARSE_OBJECT_NAME_SEPARATOR;
                                return ret;
                        } else if(!optional) {
                                return ret;
                        }
                        p->next = PARSE_OBJECT_TERMINATOR;
                        break;

                case PARSE_OBJECT_NAME_SEPARATOR:
                        p->next = consume_object_name_separator(p);
                        break;

                case PARSE_OBJECT_MEMBER_VALUE:
                        ret = parse_value(p);
                        if(is_valid(ret)) {
                                if(is_value(ret)) {
                                        p->next = PARSE_OBJECT_MEMBER_SEPARATOR;
                                }
                                return ret;
                        }
                        return ret;

                case PARSE_OBJECT_MEMBER_SEPARATOR:
                        p->next = consume_object_member_separator(p);
                        break;

                case PARSE_OBJECT_TERMINATOR:
                        return parse_object_terminator(p);

                case PARSE_EOF:
                        if(p->current == p->last)
                                return JSONN_EOF;
                        return parse_error(p);

                default:
                        return parse_error(p);
                }
        }
}

