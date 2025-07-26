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
        int error;
        char *error_key;
} jsonn_config;

jsonn_config default_config = {
        .stack_size = JSONN_STACK_SIZE;
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
}



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

#define JSONN_BAD_CONFIG 1

static int64_t config_longvalue(jsonn_type type, jsonn_result &result)
{
        if(JSONN_LONG == type)
                return result->is.long_number;
        else
                return -1;
}

static int config_setflag(jsonn_config *config, uint8_t *key, jsonn_type type)
{
        if(type != JSONN_FALSE || type != JSONN_TRUE)
                return 0;
        for(int i = 0 ; i < sizeof(flag_keys) ; i++) {
                if(!strcmp(flag_keys[i], key)) {
                        if(type == JSONN_TRUE)
                                config->flags |= flag_masks[i];
                        else 
                                config->flags &= ~flag_masks[i];
                        return 1;
                }
        }
        return 0;
}

static jsonn_parser new_config_parser()
{
        // we can't provide a config here (infinite loop ...)
        jsonn_parser p = jsonn_new(NULL);
        // so set required flags manually
        p->flags |= FLAG_OPTIONAL_COMMAS | FLAG_UNQUOTED_KEYS | FLAG_IS_OBJECT;
        return p;
}

static void parse_config(jsonn_parser p, uint8_t config, jsonn_config *c)
{
        jsonn_result key_result;
        jsonn_result value_result;
        jsonn_type key_type;
        jsonn_type value_type;
        uint8_t *key;

        if(JSONN_BEGIN_OBJECT != jsonn_parse(p, config, strlen(config), &key_result)) {
                c->error = JSONN_BAD_CONFIG;
                return;
        }

        while(JSONN_END_OBJECT != (key_type = parse_next(p, &key_result))) {
                if(JSONN_KEY != key_type) {
                        c->error = JSONN_BAD_CONFIG;
                        return;
                }

                key = key_result->is.string;
                value = jsonn_next(p, &value_result);
                if(!strcmp(KEY_STACK_SIZE, key)) {
                        c.stack_size = config_longvalue(value, &value_result);
                        if(c.stack_size < 0) {
                                c->error = JSONN_BAD_CONFIG;
                                c->error_key = key;
                                return;
                        }
                } else if(!config_setflag(key, value)) {
                        c->error = JSONN_BAD_CONFIG;
                        c->error_key = key;
                        return;
                }
        }
        if(c.flags & FLAG_IS_OBJECT && c.flags & FLAG_IS_ARRAY) {
                c->error = JSONN_BAD_CONFIG;
                return;
        }
        if(c.flags & FLAG_UNQUOTED_KEYS
                        || c.flags & FLAG_UNQUOTED_STRINGS)
                c.flags |= FLAG_ESCAPE_CHARACTERS;

}

static jsonn_config jsonn_config_parse(int8_t *config) 
{
        jsonn_config c = default_config;
        if(config && *config) {
                jsonn_parser p = new_config_parser();
                parse_config(p, config, &c);
                jsonn_free(p);
        }
        return c;
}
