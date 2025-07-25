
/*
 * Default configurations can be modified at compilation time
 * 
 * The required symbol is JSONN_ followed by uppercase of the config name
 * For example, to set the default stack size to 2048
 *
 * -DJSONN_STACK_SIZE=2048
 *
 * All boolean flags default to false and can be turned on by
 * setting the symbol without a value
 * For example, to allow comments in whitespace by default
 *
 * -DJSONN_ALLOW_COMMENTS
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
 * allow_comments
 *    C style comments are allowed within whitespace
 *
 *
 * allow_trailing_commas
 *    trailing commas are allowed in arrays and objects
 *
 *
 * replace_illformed_utf8
 *    illformed utf8 sequence are substituted by the unicode
 *    replacement character ("\EF\BD\BD" in utf8) rather then
 *    the input being rejected.
 *    In the event of multiple replacements, there may be
 *    insufficient space to make a further replacement.
 *    In this case the input will be rejected.
 *
 *
 *  allow_no_quotes
 *    strings without whitespace do not have to be quoted
 *    strings that do not start with " terminate at whitespace : , ] or }
 *    \: \, \] \} and \ followed by a space are accepted escapes
 *    strings that start with - or 0-9 must be quoted
 *
 *
 *  allow_no_commas
 *    elements of arrays and objects do not have to be separated by commas
 *
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

typedef struct {
  size_t stack_size;
  int16_t flags;
  int error;
  char *error_name;
} jsonn_config;

jsonn_config default_config = {
  .stack_size = 8 * (JSONN_STACK_SIZE / 8);
  .flags = 0x0
#ifdef JASONN_ALLOW_COMMENTS
    | FLAG_ALLOW_COMMENTS
#endif
#ifdef JSONN_ALLOW_TRAILING_COMMAS
    | FLAG_ALLOW_TRAILING_COMMAS
#endif
#ifdef JSONN_REPLACE_ILLFORMED_UTF8
    | FLAG_REPLACE_ILLFORMED_UTF8
#endif
#ifdef JSONN_ALLOW_NO_QUOTES
    | FLAG_ALLOW_NO_QUOTES
#endif
#ifdef JSONN_IS_OBJECT
    | FLAG_IS_OBJECT
#endif
#ifdef JSONN_IS_ARRAY
    | FLAG_IS_ARRAY
#endif
}

#define FLAG_ALLOW_COMMENTS         0x01
#define FLAG_ALLOW_TRAILING_COMMAS  0x02
#define FLAG_REPLACE_ILLFORMED_UTF8 0x04
#define FLAG_ALLOW_NO_QUOTES        0x08
#define FLAG_IS_OBJECT              0x10
#define FLAG_IS_OBJECT              0x20


#define NAME_STACK_SIZE             "stack_size"
#define NAME_ALLOW_COMMENTS         "allow_comments"
#define NAME_ALLOW_TRAILING_COMMAS  "allow_trailing_commas"
#define NAME_REPLACE_ILLFORMED_UTF8 "replace_illformed_utf8"
#define NAME_ALLOW_NO_QUOTES        "allow_no_quotes"
#define NAME_IS_OBJECT              "is_object"
#define NAME_IS_ARRAY               "is_array"

char *flag_names[] = {
  NAME_ALLOW_COMMENTS,
  NAME_ALLOW_TRAILING_COMMAS,
  NAME_REPLACE_ILLFORMED_UTF8,
  NAME_ALLOW_NO_QUOTES,
  NAME_IS_OBJECT,
  NAME_IS_ARRAY
};

int flag_masks[] = {
  FLAG_ALLOW_COMMENTS,
  FLAG_ALLOW_TRAILING_COMMAS,
  FLAG_REPLACE_ILLFORMED_UTF8,
  FLAG_ALLOW_NO_QUOTES,
  FLAG_IS_OBJECT,
  FLAG_IS_ARRAY
};

#define JSONN_BAD_CONFIG 1

static int64_t config_longvalue(jsonn_type type, jsonn_result &result) {
  if(JSONN_LONG == type)
    return result->is.long_number;
  else
    return -1;
}

static int config_setflag(jsonn_config *config, uint8_t *name, jsonn_type type) {
  if(type != JSONN_FALSE || type != JSONN_TRUE)
    return 0;
  for(int i = 0 ; i < sizeof(flag_names) ; i++) {
    if(!strcmp(flag_names[i], name)) {
      if(type == JSONN_TRUE)
        config->flags |= flag_masks[i];
      else 
        config->flags &= ~flag_masks[i];
      return 1;
    }
  }
  return 0;
}
  
static jsonn_parser new_config_parser() {
  // we can't provide a config here (infinite loop ...)
  jsonn_parser p = jsonn_new(NULL);
  // so set required flags manually
  p->flags |= FLAG_ALLOW_NO_QUOTES | FLAG_IS_OBJECT;
  return p;
}

static void parse_config(jsonn_parser p, uint8_t config, jsonn_config *c) {
  jsonn_result name_result;
  jsonn_result value_result;
  jsonn_type name_type;
  jsonn_type value_type;
  uint8_t *name;

  if(JSONN_BEGIN_OBJECT != jsonn_parse(p, config, strlen(config), &name_result)) {
    c->error = JSONN_BAD_CONFIG;
    return;
  }
  
  while(JSONN_END_OBJECT != (name_type = parse_next(p, &name_result))) {
    if(JSONN_NAME != name_type) {
      c->error = JSONN_BAD_CONFIG;
      return;
    }
    
    name = name_result->is.string;
    value = jsonn_next(p, &value_result);
    if(!strcmp(NAME_STACK_SIZE, name)) {
      c.stack_size = config_longvalue(value, &value_result);
      if(c.stack_size < 0) {
        c->error = JSONN_BAD_CONFIG;
        c->error_name = name;
        return;
      }
    } else if(!config_setflag(name, value)) {
        c->error = JSONN_BAD_CONFIG;
        c->error_name = name;
    }
  }
  if(JSONN_END_OBJECT != parse_next(p, &anme_result))
    c->error = JSONN_BAD_CONFIG;

  if(c.flags & FLAG_IS_OBJECT && c.flags & FLAG_IS_ARRAY)
    c->error = JSONN_BAD_CONFIG;
    
}

static jsonn_config jsonn_config_parse(int8_t *config) {
  jsonn_config c = default_config;
  if(*config && *config != '\0') {
    jsonn_parser p = new_config_parser();
    parse_config(p, config, &c);
    jsonn_free(p);
  }
  return c;
}
