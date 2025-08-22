#include <math.h>
#include <errno.h>

// TODO function naming is a mess!

#define TOKEN_INFO_DEFAULT      0x00
#define TOKEN_INFO_IS_STRING    0x01
#define TOKEN_INFO_HAS_QUOTE    0x03 // implies _IS_STRING
#define TOKEN_INFO_IS_ESCAPE    0x04
#define TOKEN_INFO_IS_SURROGATE 0x08
#define TOKEN_INFO_COPY_FORWARD 0x10

#define write_b(X, Y)   (str_buf_append(p->write_buf, (X), (Y)))
#define write_c(X)      (str_buf_append_c(p->write_buf, (X)))
#define get_content(X)  (str_buf_content(p->write_buf, (X)))

// must be same size and order as token_type
static int token_type_info[] = {
        TOKEN_INFO_DEFAULT,
        TOKEN_INFO_DEFAULT,
        TOKEN_INFO_DEFAULT,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_HAS_QUOTE,
        TOKEN_INFO_IS_STRING,
        TOKEN_INFO_IS_STRING,
        TOKEN_INFO_COPY_FORWARD,
        TOKEN_INFO_COPY_FORWARD,
        TOKEN_INFO_IS_ESCAPE,
        TOKEN_INFO_IS_ESCAPE,
        TOKEN_INFO_IS_ESCAPE | TOKEN_INFO_COPY_FORWARD,
        TOKEN_INFO_IS_SURROGATE | TOKEN_INFO_COPY_FORWARD
};

typedef enum {
        config_comments = JSONPG_FLAG_COMMENTS,
        config_trailing_commas = JSONPG_FLAG_TRAILING_COMMAS,
        config_single_quotes = JSONPG_FLAG_SINGLE_QUOTES,
        config_unquoted_keys = JSONPG_FLAG_UNQUOTED_KEYS,
        config_unquoted_strings = JSONPG_FLAG_UNQUOTED_STRINGS,
        config_escape_characters = JSONPG_FLAG_ESCAPE_CHARACTERS,
        config_optional_commas = JSONPG_FLAG_OPTIONAL_COMMAS
} config_flags;

static int push_token(jsonpg_parser p, token_type type)
{
        assert(p->token_ptr < JSONPG_TOKEN_MAX && "Token stack overflow");

        token t = &p->tokens[p->token_ptr++];
        t->type = type;
        t->pos = p->current;

        if(token_type_info[type] & TOKEN_INFO_HAS_QUOTE ) {
                // Quoted string/key starts after quote
                t->pos++;
        } else if(token_type_info[type] & TOKEN_INFO_IS_ESCAPE) {
                // Copy previous bytes from enclosing string
                assert(p->token_ptr > 0 && "Push escape token with no enclosing string");

                uint8_t *start = p->tokens[p->token_ptr - 1].pos;
                if(write_b(start, p->current - start))
                        return -1;

                // Escape starts after "\"
                t->pos++;
        }
        return 0;
}

static token pop_token(jsonpg_parser p)
{
        assert(p->token_ptr > 0 && "Token stack underflow");
        
        return &p->tokens[--p->token_ptr];
}
static jsonpg_type begin_object(jsonpg_parser p)
{
        return(0 == push_stack(&p->stack, STACK_OBJECT))
                ? JSONPG_BEGIN_OBJECT
                : set_result_error(p, JSONPG_ERROR_STACKOVERFLOW);
}

static jsonpg_type end_object(jsonpg_parser p)
{
        return (0 == pop_stack(&p->stack))
                ? JSONPG_END_OBJECT
                : set_result_error(p, JSONPG_ERROR_STACKUNDERFLOW);
}

static jsonpg_type begin_array(jsonpg_parser p)
{
        return(0 == push_stack(&p->stack, STACK_ARRAY))
                ? JSONPG_BEGIN_ARRAY
                : set_result_error(p, JSONPG_ERROR_STACKOVERFLOW);
}

static jsonpg_type end_array(jsonpg_parser p)
{
        return (0 == pop_stack(&p->stack))
                ? JSONPG_END_ARRAY
                : set_result_error(p, JSONPG_ERROR_STACKUNDERFLOW);
}

static jsonpg_type accept_integer(jsonpg_parser p, token t)
{
        errno = 0;
        long integer = strtol((char *)t->pos, NULL, 10);
        if(errno) 
                return number_error(p);
        p->result.number.integer = integer;
        return JSONPG_INTEGER;
}

static jsonpg_type accept_real(jsonpg_parser p, token t)
{
        errno = 0;
        double real = strtod((char *)t->pos, NULL);
        if(errno) 
                return number_error(p);

        if(!(real == 0 || isnormal(real))) 
                return number_error(p);

        p->result.number.real = real;
        return JSONPG_REAL;
}

static int set_string_value(jsonpg_parser p, token t)
{
        if(p->write_buf->count) {
                if(write_b(t->pos, p->current - t->pos))
                        return -1;
                p->result.string.length = get_content(&p->result.string.bytes);
        } else {
                p->result.string.bytes = t->pos;
                p->result.string.length = p->current - t->pos;
        }
        return 0;
}

static jsonpg_type accept_string(jsonpg_parser p, token t)
{
        if(set_string_value(p, t))
                return alloc_error(p);
        return JSONPG_STRING;
}

static jsonpg_type accept_key(jsonpg_parser p, token t)
{
        if(set_string_value(p, t))
                return alloc_error(p);
        return JSONPG_KEY;
}

static void reset_string_after_escape(jsonpg_parser p)
{
        // After escape set enclosing string token.pos
        // to point to after the escape sequence
        assert(p->token_ptr > 0 && "Pop escape token with no enclosing string");
        
        p->tokens[p->token_ptr - 1].pos = p->current + 1;
}

static int process_escape(jsonpg_parser p, token t)
{
        // Only JSON specified escape chars get here
        static char *escapes = "bfnrt\"\\/";
        char *c = strchr(escapes, *t->pos);

        assert(c && "Invalid escape character");

        if(write_c("\b\f\n\r\t\"\\/"[c - escapes]))
                return -1;

        reset_string_after_escape(p);
        return 0;
}

static int process_escape_chars(jsonpg_parser p, token t)
{
        if(write_c(*t->pos))
                return -1;

        reset_string_after_escape(p);
        return 0;
}

static int parse_4hex(uint8_t *hex_ptr) {
        int value = 0;
        for(int i = 0 ; i < 4 ; i++) {
                int v = *hex_ptr++;
                if(v >= '0' && v <= '9') {
                        v -= '0';
                } else if(v >= 'A' && v <= 'F') {
                        v -= 'A' - 10;
                } else if(v >= 'a' && v <= 'f') {
                        v -= 'a' - 10;
                } else {
                        assert(0 && "Invalid hex character");
                }
                value = (value << 4) | v;
        }
        return value;
}

static int process_escape_u(jsonpg_parser p, token t)
{
        // \uXXXX or \uXXXX\uXXXX
        // token points to first "u"
        int cp = parse_4hex(1 + t->pos);
        if(p->current - t->pos > 9)
                cp = surrogate_pair_to_codepoint(cp, parse_4hex(7 + t->pos));
        if(write_utf8_codepoint(cp, p->write_buf))
                return -1;

        reset_string_after_escape(p);
        return 0;
}

static int input_read(jsonpg_parser p, uint8_t *start)
{
       uint8_t *pos = start;
       int max = p->input_size - (start - p->input);
       while(max) {
               int l = p->reader->read(p->reader, pos, max);
               if(l < 0)
                       return -1;
               if(l == 0)
                       break;
               pos += l;
               max -= l;
       }
       p->last = pos;
       p->current = start;
       
       return max == 0;
}

static int parser_read_next(jsonpg_parser p)
{
        assert(p->input_is_ours && "Cannot write to user supplied buffer");

        uint8_t *start = p->input;
        if(p->token_ptr > 0) {
                // We have a token on the stack
                // Perform any special processing required
                // to survive the crossing of input buffer boundaries
                token t = &p->tokens[p->token_ptr - 1];
                int tinfo = token_type_info[t->type];
                if(tinfo & TOKEN_INFO_COPY_FORWARD) {
                        // copy bytes from current input needed
                        // for parsing next input
                        uint8_t *tpos;
                        if(tinfo & TOKEN_INFO_IS_SURROGATE) {
                                // sub-token of escape_u
                                // and we need to copy entire escape sequence
                                assert(p->token_ptr > 1 
                                                && "Surrogate token without parent");
                                tpos = p->tokens[p->token_ptr - 2].pos;
                        } else {
                                tpos = t->pos;
                        }
                        while(tpos < p->last)
                                *start++ = *tpos++;
                        
                } else if(tinfo & TOKEN_INFO_IS_STRING) {
                        // write string bytes as we are still in a string
                        if(write_b(t->pos, p->last - t->pos))
                                return -1;
                        // And adjust token to start of string continuation
                        t->pos = start;

                }
        }
        int l = input_read(p, start);
        if(l >= 0)
                p->seen_eof = (l == 0);

        return l;
}

static ssize_t read_fd(jsonpg_reader r, void *buf, size_t count)
{
        return read(CTX_TO_INT(r->ctx), buf, count);
}

jsonpg_parser jsonpg_parser_new(jsonpg_config *config)
{
        jsonpg_config c = config_select(config); 

        size_t struct_bytes = sizeof(struct jsonpg_parser_s);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (c.stack_size + 7) / 8;
        jsonpg_parser p = jsonpg_alloc(struct_bytes + stack_bytes);
        if(p) {
                p->write_buf = str_buf_empty();
                if(!p->write_buf) {
                        jsonpg_dealloc(p);
                        return NULL;
                }
                p->reader = NULL;
                p->input = NULL;
                p->input_is_ours = 0;
                p->stack.size = c.stack_size;
                p->stack.stack = (uint8_t *)(((void *)p) + struct_bytes);
                p->flags = c.flags;

                if(c.flags & JSONPG_FLAG_IS_OBJECT) {
                        p->stack.ptr = 0;
                        push_stack(&p->stack, STACK_OBJECT);
                        p->stack.ptr_min = 1;
                } else if(c.flags & JSONPG_FLAG_IS_ARRAY) {
                        p->stack.ptr = 0;
                        push_stack(&p->stack, STACK_ARRAY);
                        p->stack.ptr_min = 1;
                } else {
                        p->stack.ptr_min = 0;
                }
        }
        return p;
}

void jsonpg_parser_free(jsonpg_parser p) 
{
        if(p) {
                if(p->input_is_ours)
                        jsonpg_dealloc(p->input);
                jsonpg_dealloc(p->reader);
                str_buf_free(p->write_buf);
                jsonpg_dealloc(p);
        }
}

static jsonpg_type parse(
                jsonpg_parser p,
                jsonpg_generator g)
{
        jsonpg_type type;
        int abort = 0;
        while(!abort && JSONPG_EOF != (type = jsonpg_parse_next(p))) {
                abort = generate(g, type, &p->result);
        }

        return type;
}

jsonpg_type jsonpg_parse(
                jsonpg_parser p, 
                uint8_t *json, 
                uint32_t length,
                jsonpg_generator g)
{
        if(p->reader) {
                jsonpg_dealloc(p->reader);
                p->reader = NULL;
        }
        p->write_buf = str_buf_reset(p->write_buf);
        if(p->input_is_ours)
                jsonpg_dealloc(p->input);

        p->input = p->current = json;
        p->input_size = length;
        p->input_is_ours = 0;
        p->last = json + length;
        p->seen_eof = 1;
        p->stack.ptr = p->stack.ptr_min;
        p->token_ptr = 0;
        p->state = JSONPG_STATE_INITIAL;

        // Skip leading byte order mark
        p->current += bom_bytes(p->input, p->input_size);

        return g
                ? parse(p, g)
                : JSONPG_ROOT;
}

jsonpg_type jsonpg_parse_str(
                jsonpg_parser p,
                char *json,
                jsonpg_generator g)
{
        return jsonpg_parse(p, (uint8_t *)json, strlen(json), g);
}

jsonpg_type jsonpg_parse_reader(
                jsonpg_parser p,
                jsonpg_reader r,
                jsonpg_generator g)
{
        if(!(p->input_is_ours && p->input)) {
                p->input = jsonpg_alloc(JSONPG_BUF_SIZE);
                if(!p->input)
                        return alloc_error(p);
                p->input_size = JSONPG_BUF_SIZE;
                p->input_is_ours = 1;
        }
        p->write_buf = str_buf_reset(p->write_buf);
        p->reader = r;
        int l = input_read(p, p->input);
        if(l < 0)
                return file_read_error(p);
        p->seen_eof = (0 == l);
        p->stack.ptr = p->stack.ptr_min;
        p->token_ptr = 0;
        p->state = JSONPG_STATE_INITIAL;

        // Skip leading byte order mark
        p->current += bom_bytes(p->input, p->input_size);

        return g
                ? parse(p, g)
                : JSONPG_ROOT;
}

jsonpg_reader jsonpg_file_reader(int fd)
{
        jsonpg_reader r = jsonpg_alloc(sizeof(struct jsonpg_reader_s));
        if(!r)
                return NULL;
        r->read = read_fd;
        r->ctx = INT_TO_CTX(fd);

        return r;
}

jsonpg_type jsonpg_parse_fd(
                jsonpg_parser p, 
                int fd, 
                jsonpg_generator g)
{
        jsonpg_reader r = jsonpg_file_reader(fd);
        if(!r)
                return alloc_error(p);
        return jsonpg_parse_reader(p, r, g);
}

jsonpg_type jsonpg_parse_stream(
                jsonpg_parser p, 
                FILE *stream, 
                jsonpg_generator g)
{
        return jsonpg_parse_fd(p, fileno(stream), g);
}

jsonpg_value jsonpg_result(jsonpg_parser p)
{
        return p->result;
}
