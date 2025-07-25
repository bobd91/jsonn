#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jsonn.h"

typedef enum {
        // Don't change as the first two values are used for stack bit
        PARSE_OBJECT_MEMBER_SEPARATOR = 0,
        PARSE_ARRAY_VALUE_SEPARATOR = 1,
        PARSE_OBJECT_MEMBER_OPTIONAL,
        PARSE_ARRAY_VALUE_OPTIONAL,
        PARSE_OBJECT_MEMBER,
        PARSE_ARRAY_VALUE,
        PARSE_OBJECT_NAME_SEPARATOR,
        PARSE_OBJECT_MEMBER_VALUE,
        PARSE_OBJECT_TERMINATOR,
        PARSE_ARRAY_TERMINATOR,
        PARSE_NAME,
        PARSE_VALUE,
        PARSE_UNEXPECTED,
        PARSE_EOF
} parse_next;

static parse_next jsonn_init_next(jsonn_parser p)
{
        if(p->flags & FLAG_IS_OBJECT)
                return PARSE_OBJECT_MEMBER;
        else if(p->flags & FLAG_IS_ARRAY)
                return PARSE_ARRAY_VALUE;
        else
                return PARSE_VALUE;
}

static parse_next pop_next(jsonn_parser p) 
{
        p->stack_pointer--;
        size_t sp = p->stack_pointer;
        if(sp < 0)
                return PARSE_ERROR;
        p->next = 0x01 & p->stack[sp >> 3] >> (sp & 0x07);
        return p->next;
}

static parse_next push_next(jsonn_parser p, parse_next next) 
{
        // Only allow PARSE_[OBJECT_MEMBER|ARRAY_VALUE]_SEPARATOR
        // which are 0 and 1
        assert(!next >> 1);
        size_t sp = p->stack_pointer;
        if(sp >= p->stack_size) 
                return PARSE_ERROR;
        int offset = sp >> 3;
        int mask = 1 << (sp & 0x07);

        if(next)
                p->stack[offset] |= mask;
        else
                p->stack[offset] &= ~mask;

        p->stack_pointer++;
        p->next = next;
        return next;
}

static jsonn_type begin_object(jsonn_parser p) 
{
        p->current++;
        return(push_next(p, PARSE_OBJECT_MEMBER_OPTIONAL) != PARSE_ERROR)
                ? JSONN_BEGIN_OBJECT
                : JSONN_UNEXPECTED;
}

static jsonn_type end_object(jsonn_parser p) 
{
        p->current++;
        return(pop_next(p) != PARSE_ERROR)
                ? JSONN_END_OBJECT
                : JSONN_UNEXPECTED;
}

static jsonn_type begin_array(jsonn_parser p) 
{
        p->current++;
        return(push_next(p, PARSE_ARRAY_VALUE_OPTIONAL) != PARSE_ERROR)
                ? JSONN_BEGIN_ARRAY
                : JSONN_UNEXPECTED;
}

static jsonn_type end_array(jsonn_parser p) 
{
        p->current++;
        return(pop_next(p) != PARSE_ERROR)
                ? JSONN_END_ARRAY
                : JSONN_UNEXPECTED;
}

static jsonn_type parse_literal(
                jsonn_parser p, 
                char *literal, 
                jsonn_type type) 
{
        // Already matched first character of literal
        char *s1 = literal;
        char *s2 = p->current;
        while(*++s1 == *++s2 && *s1)
                ;
        // If we are at the null at the end of literal then we found it
        if(!*s1) {
                p->current = s2;
                return type;
        }
        return JSONN_UNEXPECTED;
}

static jsonn_type parse_value(jsonn_parser p, jsonn_value *result) 
{
        if(p->current < p->last) {
                switch(*p->current) {
                case '"':
                        return parse_string(p, result);
                case '{':
                        return begin_object(p);
                case '[':
                        return begin_array(p);
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
                        return parse_number(p, result);
                }
        }
        return JSONN_EOF;
}

static jsonn_type parse_array_terminator(jsonn_parser p, jsonn_value *result) 
{
        if(p->current < p->last) {
                if(']' == *p->current) {
                        p->current++;
                        return end_array(p);
                }
        }
        return JSONN_UNEXPECTED;
}

static jsonn_type parse_object_terminator(jsonn_parser p, jsonn_value *result) 
{
        if(p->current < p->last) {
                if('}' == *p->current) {
                        p->current++;
                        return end_object(p);
                }
        }
        return JSONN_UNEXPECTED;
}


static parse_next consume_array_value_separator(jsonn_parser p)
{
        if(p->current < p->last) {
                switch(*p->current) {
                case ',':
                        p->current++;
                        return (p->flags && FLAG_ALLOW_TRAILING_COMMAS)
                                ? PARSE_ARRAY_VALUE_OPTIONAL
                                : PARSE_ARRAY_VALUE;
                case ']':
                        // not separator so don't consume
                        return PARSE_ARRAY_TERMINATOR;
                }
        }
        return (p->flags & FLAG_IS_ARRAY)
                ? PARSE_EOF
                : PARSE_UNEXPECTED;
}

static parse_next consume_object_member_separator(jsonn_parser p) 
{
        if(p->current < p->last) {
                switch(*p->current) {
                case ',':
                        p->current++;
                        return (p->flags && FLAG_ALLOW_TRAILING_COMMAS)
                                ? PARSE_OBJECT_MEMBER_OPTIONAL
                                : PARSE_OBJECT_MEMBER;
                case '}':
                        // not separator so don't consume
                        return PARSE_OBJECT_TERMINATOR;
                }
        }
        return (p->flags & FLAG_IS_OBJECT)
                ? PARSE_EOF
                : PARSE_UNEXPECTED;
}

static parse_next consume_object_name_separator(jsonn_parser p) 
{
        if(p->current < p->last) {
                if(':' == *p->current) {
                        p->current++;
                        return PARSE_OBJECT_MEMBER_VALUE;
                }
        }
        return PARSE_UNEXPECTED;
}

static int is_valid(jsonn_type type) 
{
        return type != JSONN_UNEXPECTED;
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

static jsonn_type jsonn_parse_next(jsonn_parser p, jsonn_value *result)
{
        jsonn_type ret;
        while(1) {
                consume_whitespace(p);
                int optional = 1; // are values/pairs required?
                switch(p->next) {

                case PARSE_VALUE:
                        ret = parse_value(p, result);
                        p->next = PARSE_EOF;
                        return ret;

                case PARSE_ARRAY_VALUE:
                        optional = 0;
                        // fall through
                case PARSE_ARRAY_VALUE_OPTIONAL:
                        ret = parse_value(p, result);
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
                        return parse_array_terminator(p, result);

                case PARSE_OBJECT_MEMBER:
                        optional = 0;
                        // fall through
                case PARSE_OBJECT_MEMBER_OPTIONAL:
                        ret = parse_name(p, result);
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
                        ret = parse_value(p, result);
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
                        return parse_object_terminator(result);

                case PARSE_EOF:
                        if(p->current == p->last)
                                return JSONN_EOF;
                        else
                                return JSONN_UNEXPECTED;

                default:
                        return JSONN_UNEXPECTED;
                }
        }
}

