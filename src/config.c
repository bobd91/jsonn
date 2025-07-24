
/*
 * Default configurations can be modified at compilation time
 * 
 * The required symbol is JSONN_ followed by uppercase of the config name
 * 
 * -DJSONN_MAX_DEPTH=2048
 *  sets the default max_depth to 2048
 *
 *  All boolean flags default to false and can be turned on by
 *  setting the symbol without a value
 *
 *  -DJSONN_ALLOW_COMMENTS
 *  to allow comments in whitespace
 *
 * max_depth
 *    maximum stack depth for recording nesting of [] and {}
 *    memory required is max_depth bits
 *    default: CONFIG_MAX_DEPTH=1024
 *
 * stream_buffer_size
 *    amount of memory allocated to buffering streamed input
 *    default: CONFIG_STREAM_BUFFER_SIZE=2048
 *
 * Flags:
 *
 * allow_comments
 *    C style comments are allowed within whitespace
 *
 * allow_trailing_commas
 *    trailing commas are allowed in [] and {}
 *
 * replace_illformed_utf8
 *    illformed utf8 sequence are substituted by the unicode
 *    replacement character ("\EF\BD\BD" in utf8)
 *    In the event of multiple llformed sequences there may be
 *    insufficient space to make a further replacement.
 *    In this case the input will be rejected.
 *
 *  allow_no_quotes
 *    strings do not have to be quoted
 *    strings terminate at , ] or }
 *    \, \] and \} are accepted escapes
 *
 *  is_object
 *    treats the input as being surrounded by { }
 *    with allow_no_quotes above this can parse input such as
 *    xx=true, yy=12, zz=top
 *    jsonn uses these flags to parse its own configuration
 */

#ifndef JSONN_MAX_DEPTH
#define JSONN_MAX_DEPTH 1024
#endif

#ifndef JSONN_STREAM_BUFFER_SIZE
#define JSONN_STREAM_BUFFER_SIZE 2048
#endif

typedef struct {
  size_t max_depth;
  size_t buffersize;
  int16_t flags;
  int error;
  char *error_name;
} jsonn_config;

jsonn_config default_config = {
  .max_depth = JSONN_MAX_DEPTH,
  .buffersize = JSONN_STREAM_BUFFER_SIZE,
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
}

#define FLAG_ALLOW_COMMENTS         0x01
#define FLAG_ALLOW_TRAILING_COMMAS  0x02
#define FLAG_REPLACE_ILLFORMED_UTF8 0x04
#define FLAG_ALLOW_NO_QUOTES        0x08
#define FLAG_IS_OBJECT              0x10


#define NAME_MAX_DEPTH              "max_depth"
#define NAME_STREAM_BUFFER_SIZE     "stream_buffer_size"
#define NAME_ALLOW_COMMENTS         "allow_comments"
#define NAME_ALLOW_TRAILING_COMMAS  "allow_trailing_commas"
#define NAME_REPLACE_ILLFORMED_UTF8 "replace_illformed_utf8"
#define NAME_ALLOW_NO_QUOTES        "allow_no_quotes"
#define NAME_IS_OBJECT              "is_object"

char *flag_names[] = {
  NAME_ALLOW_COMMENTS,
  NAME_ALLOW_TRAILING_COMMAS,
  NAME_REPLACE_ILLFORMED_UTF8,
  NAME_ALLOW_NO_QUOTES,
  NAME_IS_OBJECT
};

int flag_masks[] = {
  FLAG_ALLOW_COMMENTS,
  FLAG_ALLOW_TRAILING_COMMAS,
  FLAG_REPLACE_ILLFORMED_UTF8,
  FLAG_ALLOW_NO_QUOTES,
  FLAG_IS_OBJECT
};

#define JSONN_BAD_CONFIG 1

static int64_t config_longvalue(jsonn_type type, jsonn_result &result) {
  if(JSONN_LONG == type)
    return result->is>long_number;
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
  
static jsonn_parser config_parser(uint8_t *config) {
  // we can't provide a config here (infinite loop ...)
  jsonn_parser p = jsonn_buffer_parser(NULL);
  // so set the flags manually
  p->flags |= FLAG_ALLOW_NO_QUOTES | FLAG_SIMPLE_OBJECT;
  p->jsonn_init_buffer(p, config, strlen(config));
  return p;
}

static void parse_config(jsonn_parser p, jsonn_config *c) {
  jsonn_result name_result;
  jsonn_result value_result;
  jsonn_type name_type;
  uint8_t *name;
  jsonn_type value_type;

  if(JSONN_BEGIN_OBJECT != parse_next(p, &name_result)) {
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
    if(!strcmp(NAME_MAX_DEPTH, name)) {
      c.max_depth = config_longvalue(value, &value_result);
      if(c.max_depth < 0) {
        c->error = JSONN_BAD_CONFIG;
        c->error_name = name;
        return;
      }
    } else if(!strcmp(NAME_STREAM_BUFFER_SIZE, name)) {
      c.buffersize = config_longvalue(value, &value_result);
      if(c.buffersize < 0) {
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
    
}

jsonn_config jsonn_config_parse(int8_t *config) {
  jsonn_config c = default_config;
  if(*config && *config != '\0') {
    jsonn_parser p = config_parser(config);
    parse_config(p, &c);
    jsonn_free(p);
  }
  return c;
}
