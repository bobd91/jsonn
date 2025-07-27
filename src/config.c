/*
 * Default configurations can be modified at compilation time
 * 
 * The required symbol is JSONN_ followed by uppercase of the config key
 * For example, to set the default stack size to 2048
 *
 * -DJSONN_STACK_SIZE=2048
 *
 * All boolean flags default to false and can be turned on by
 * setting the symbol without a value
 * For example, to allow comments in whitespace by default
 *
 * -DJSONN_COMMENTS
 *
 *
 * stack_size
 *    size of stack for recording nesting of [] and {}
 *    memory used is (stack_size/8) bytes
 *    default: JSONN_STACK_SIZE=1024
 *
 *
 * Flags:
 *
 * replace_illformed_utf8
 *      the Unicode standard recommends that illformed utf8
 *      sequences by replaced by the unicode replacement character
 *      default behaviour is to reject illformed utf8
 * 
 *      In the event of multiple replacements, there may be
 *      insufficient space to make a further replacement.
 *      In this case the parse will fail.
 *
 * comments
 *      C style comments are allowed within whitespace
 *
 * trailing_commas
 *      trailing commas are allowed in arrays and objects
 *
 * single_quotes
 *      strings can be delimited by single quote
 *
 * unquoted_keys
 *      object keys do not have to be quoted
 *      unquoted keys are terminated by : or whitespace
 *
 *      the 'escape_characters' option will be enabled so
 *      that \: and \<space> will be valid input
 *
 * unquoted_strings
 *      strings do not have to be quoted
 *      unquoted strings are terminated by , } ] or whitespace
 *
 *      the 'escape_characters' option will be enabled so
 *      that \, \} \] and \<space> will be valid input
 *
 *      strings containing JSON unicode escapes must always
 *      be quoted
 *
 *      if the string could otherwise be interpreted as a valid
 *      JSON value then the first character should be escaped
 *
 *      a : 123    => "a" : 123
 *      a : \123   => "a" : "123" 
 *
 * escape_characters
 *      within strings any character can be escaped
 *      this includes newline characters so strings can be
 *      entered over multiple lines
 *
 *      within quoted strings the JSON specified escapes are honored
 *      a: \null   => "a" : "null"
 *      a: "\null" => "a" : "\null" 
 *
 *  optional_commas
 *    elements of arrays and objects do not have to be separated
 *    by commas
 *
 *  is_object
 *    treats the input as being surrounded by { }
 *
 *
 *  is_array
 *    treats the input as being surrounded by [ ]
 *
 *  Note: a configuration with both is_object and is_array will be rejected
 */

#include <string.h>

#include "jsonn.h"
#include "parse.h"

#ifndef JSONN_STACK_SIZE
#define JSONN_STACK_SIZE 1024
#endif

#define FLAG_REPLACE_ILLFORMED_UTF8     0x01
#define FLAG_COMMENTS                   0x02
#define FLAG_TRAILING_COMMAS            0x04
#define FLAG_SINGLE_QUOTES              0x08
#define FLAG_UNQUOTED_KEYS              0x10
#define FLAG_UNQUOTED_STRINGS           0x20
#define FLAG_ESCAPE_CHARACTERS          0x40
#define FLAG_OPTIONAL_COMMAS            0x80
#define FLAG_IS_OBJECT                  0x100
#define FLAG_IS_ARRAY                   0x200

typedef struct {
        size_t stack_size;
        int16_t flags;
} parser_config;

parser_config default_config = {
        .stack_size = JSONN_STACK_SIZE,
        .flags = 0x0
#ifdef JSONN_REPLACE_ILLFORMED_UTF8
                | FLAG_REPLACE_ILLFORMED_UTF8
#endif
#ifdef JASONN_COMMENTS
                | FLAG_COMMENTS
#endif
#ifdef JSONN_TRAILING_COMMAS
                | FLAG_TRAILING_COMMAS
#endif
#ifdef JSONN_SINGLE_QUOTES
                | FLAG_SINGLE_QUOTES
#endif
#ifdef JSONN_UNQUOTED_KEYS
                | FLAG_UNQUOTED_KEYS
#endif
#ifdef JSONN_UNQUOTED_STRINGS
                | FLAG_UNQUOTED_STRINGS
#endif
#ifdef JSONN_ESCAPE_CHARACTERS
                | FLAG_ESCAPE_CHARACTERS
#endif
#ifdef JSONN_OPTIONAL_COMMAS
                | FLAG_OPTIONAL_COMMAS
#endif
#ifdef JSONN_IS_OBJECT
                | FLAG_IS_OBJECT
#endif
#ifdef JSONN_IS_ARRAY
                | FLAG_IS_ARRAY
#endif
};



#define KEY_STACK_SIZE                 "stack_size"
#define KEY_REPLACE_ILLFORMED_UTF8     "replace_illformed_utf8"
#define KEY_COMMENTS                   "comments"
#define KEY_TRAILING_COMMAS            "trailing_commas"
#define KEY_SINGLE_QUOTES              "single_quotes"
#define KEY_UNQUOTED_KEYS              "unquoted_keys"
#define KEY_UNQUOTED_STRINGS           "unquoted_strings"
#define KEY_ESCAPE_CHARACTERS          "escape_characters"
#define KEY_OPTIONAL_COMMAS            "optional_commas"
#define KEY_IS_OBJECT                  "is_object"
#define KEY_IS_ARRAY                   "is_array"

char *flag_keys[] = {
        KEY_REPLACE_ILLFORMED_UTF8,
        KEY_COMMENTS,
        KEY_TRAILING_COMMAS,
        KEY_SINGLE_QUOTES,
        KEY_UNQUOTED_KEYS,
        KEY_UNQUOTED_STRINGS,
        KEY_ESCAPE_CHARACTERS,
        KEY_OPTIONAL_COMMAS,
        KEY_IS_OBJECT,
        KEY_IS_ARRAY
};

int flag_masks[] = {
        FLAG_REPLACE_ILLFORMED_UTF8,
        FLAG_COMMENTS,
        FLAG_TRAILING_COMMAS,
        FLAG_SINGLE_QUOTES,
        FLAG_UNQUOTED_KEYS,
        FLAG_UNQUOTED_STRINGS,
        FLAG_ESCAPE_CHARACTERS,
        FLAG_OPTIONAL_COMMAS,
        FLAG_IS_OBJECT,
        FLAG_IS_ARRAY
};

static int config_flag(char *key)
{
        for(int i = 0 ; i < sizeof(flag_keys) ; i++) {
                if(!strcmp(flag_keys[i], key)) {
                        return flag_masks[i];
                }
        }
        return 0;
}

static jsonn_parser new_config_parser(jsonn_error *error)
{
        // we can't provide a config here (or we infinite loop)
        jsonn_parser p = jsonn_new(NULL, error);
        if(p)
                // so set required flags manually
                p->flags |= FLAG_OPTIONAL_COMMAS 
                          | FLAG_UNQUOTED_KEYS 
                          | FLAG_IS_OBJECT;
        return p;
}

// Return 1 on successful parse, otherwise 0
static int parse_config(jsonn_parser p, char *config, parser_config *c)
{
        jsonn_type key_type;
        jsonn_type value_type;
        char *key;
        int flag;

        if(JSONN_BEGIN_OBJECT != jsonn_parse(p, (uint8_t *)config, strlen(config), NULL)) {
                return 0;
        }

        while(JSONN_END_OBJECT != (key_type = jsonn_parse_next(p))) {
                if(JSONN_KEY != key_type)
                        return 0;

                key = (char *)p->result.is.string.bytes;
                value_type = jsonn_parse_next(p);
                if(!strcmp(KEY_STACK_SIZE, key)) {
                        if(JSONN_LONG != value_type)
                                return 0;

                        c->stack_size = p->result.is.number.long_value;
                        if(c->stack_size < 0) 
                                return 0;
                        
                } else if(0 != (flag = config_flag(key))) {
                        switch(value_type) {
                        case JSONN_TRUE:
                                c->flags |= flag;
                                break;
                        case JSONN_FALSE:
                                c->flags &= ~flag;
                                break;
                        default:
                                return 0;
                        }
                } else {
                        return 0;
                }
        }

        // Can't be both object and array
        if(c->flags & FLAG_IS_OBJECT && c->flags & FLAG_IS_ARRAY)
                return 0;

        // Allow escpare_characters if unquoted allowed
        if(c->flags & (FLAG_UNQUOTED_KEYS | FLAG_UNQUOTED_STRINGS))
                c->flags |= FLAG_ESCAPE_CHARACTERS;
        
        return 1;
}

// Need to check error->code after calling
static parser_config config_parse(char *config, jsonn_error *error) 
{
        parser_config c = default_config;
        if(config && *config) {
                jsonn_parser p = new_config_parser(error);
                if(p) {
                        if(!parse_config(p, config, &c)) {
                                error->code = JSONN_ERROR_CONFIG;
                                error->at = p->current - p->start;
                        }
                        jsonn_free(p);
                }
        }
        return c;
}
