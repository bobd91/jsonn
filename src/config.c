/*
 * Users can set the configuration for a specific parse
 * or reset the default configuration for all perses 
 * in the current program execution
 * 
 * jsonn_config_get() gets the default config
 *
 * individual fields can then be set
 *
 * json_config_set() will overwrite the default config
 * used for all future calls
 *
 * jsonn_new() can be called with a custom config or NULL 
 * to use the default config
 *
 *
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
 *  Note: a configuration with both is_object and is_array will enable neither 
 */

#include <string.h>

#include "jsonn.h"

static jsonn_config config_defaults = {
        .stack_size = JSONN_STACK_SIZE,
        .flags = 0x0
#ifdef JSONN_REPLACE_ILLFORMED_UTF8
                | JSONN_FLAG_REPLACE_ILLFORMED_UTF8
#endif
#ifdef JASONN_COMMENTS
                | JSONN_FLAG_COMMENTS
#endif
#ifdef JSONN_TRAILING_COMMAS
                | JSONN_FLAG_TRAILING_COMMAS
#endif
#ifdef JSONN_SINGLE_QUOTES
                | JSONN_FLAG_SINGLE_QUOTES
#endif
#ifdef JSONN_UNQUOTED_KEYS
                | JSONN_FLAG_UNQUOTED_KEYS
#endif
#ifdef JSONN_UNQUOTED_STRINGS
                | JSONN_FLAG_UNQUOTED_STRINGS
#endif
#ifdef JSONN_ESCAPE_CHARACTERS
                | JSONN_FLAG_ESCAPE_CHARACTERS
#endif
#ifdef JSONN_OPTIONAL_COMMAS
                | JSONN_FLAG_OPTIONAL_COMMAS
#endif
#ifdef JSONN_IS_OBJECT
                | JSONN_FLAG_IS_OBJECT
#endif
#ifdef JSONN_IS_ARRAY
                | JSONN_FLAG_IS_ARRAY
#endif
};


jsonn_config jsonn_config_get()
{
        jsonn_config config = {
                .stack_size = config_defaults.stack_size,
                .flags = config_defaults.flags
        };

        return config;
}

void jsonn_config_set(jsonn_config *config) {
        config_defaults.stack_size = config->stack_size;
        config_defaults.flags = config->flags;
}

static jsonn_config config_select(jsonn_config *chosen_config) {
        jsonn_config config = {
                .stack_size = JSONN_STACK_SIZE,
                .flags = 0
        };

        jsonn_config *source_config = chosen_config 
                ? chosen_config 
                : &config_defaults;

        if(source_config->stack_size >= 0)
                config.stack_size = source_config->stack_size;


        uint16_t incompatible_flags = JSONN_FLAG_IS_OBJECT | JSONN_FLAG_IS_ARRAY;
        if(incompatible_flags == 
                        (source_config->flags & incompatible_flags))
                // is_object and is_array both set so unset both
                config.flags = source_config->flags & ~incompatible_flags;
        else
                config.flags = source_config->flags;

        return config;
}
