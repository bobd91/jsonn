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
 *      enable the 'escape_characters' option to
 *      allow \: and \<space> as valid escapes
 *
 * unquoted_strings
 *      strings do not have to be quoted
 *      unquoted strings are terminated by , } ] or whitespace
 *
 *      enable the 'escape_characters' option to
 *      allow \, \} \] and \<space> as valid escapes
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

#include "error.c"

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

// Default config for user parser
static parser_config jsonn_parser_config = {
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

// Config for parsing configuration
static const parser_config config_parser_config = {
        .stack_size = 0,
        .flags = FLAG_OPTIONAL_COMMAS
                | FLAG_UNQUOTED_KEYS 
                | FLAG_IS_OBJECT
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
        size_t bytes = sizeof(flag_keys) / sizeof(flag_keys[0]);
        for(int i = 0 ; i < bytes ; i++) {
                if(!strcmp(flag_keys[i], key)) {
                        return flag_masks[i];
                }
        }
        return 0;
}

// Each parser will want to modify the default config
static void config_copy_defaults(parser_config *config) {
        config->stack_size = jsonn_parser_config.stack_size;
        config->flags = jsonn_parser_config.flags;
}

// Config parsing always uses the same config
static const parser_config *config_config() {
        return &config_parser_config;
}
#include <stdio.h>
// Return JSONN_EOF on successful parse, otherwise JSONN_ERROR
static jsonn_type config_parse(jsonn_parser p, parser_config *c)
{
        jsonn_type key_type;
        jsonn_type value_type;
        char *key;
        int flag;

        jsonn_parse(p, p->extra, strlen((char *)p->extra), NULL);
        while(JSONN_EOF != (key_type = jsonn_parse_next(p))) {
                if(JSONN_KEY != key_type)
                        return config_error(p);

                key = (char *)p->result.is.string.bytes;
                if(!strcmp(KEY_STACK_SIZE, key)) {
                        value_type = jsonn_parse_next(p);
                        if(JSONN_LONG != value_type)
                                return config_error(p);

                        c->stack_size = p->result.is.number.long_value;
                        if(c->stack_size < 0) 
                                return config_error(p);
                        
                } else if(0 != (flag = config_flag(key))) {
                        value_type = jsonn_parse_next(p);
                        switch(value_type) {
                        case JSONN_TRUE:
                                c->flags |= flag;
                                break;
                        case JSONN_FALSE:
                                c->flags &= ~flag;
                                break;
                        default:
                                return config_error(p);
                        }
                } else {
                        return config_error(p);
                }
        }

        // Can't be both object and array
        if(c->flags & FLAG_IS_OBJECT && c->flags & FLAG_IS_ARRAY)
                return config_error(p);
        
        return JSONN_EOF;
}
