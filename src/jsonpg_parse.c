
// TODO function naming is a mess!

#define STACK_OBJECT    0
#define STACK_ARRAY     1

static void dump_p(jsonpg_parser p)
{
        fprintf(stderr, "Parser Error:\n");
        fprintf(stderr, "Error: %d\n", p->result.error.code);
        fprintf(stderr, "At Position: %ld\n", p->result.error.at);
        fprintf(stderr, "Input Length: %ld\n", p->last - p->start);
        fprintf(stderr, "Input Processed: %ld\n", p->current - p->start);
        fprintf(stderr, "Next State: %d\n", p->next);
        fprintf(stderr, "Stack Size: %ld\n", p->stack_size);
        fprintf(stderr, "Stack Pointer: %ld\n", p->stack_ptr);
        fprintf(stderr, "Stack: ");
        for(int i = 0 ; i < p->stack_ptr ; i++) {
                int offset = i >> 3;
                int mask = 1 << (i & 0x07);
                fprintf(stderr, "%c", 
                                (mask & p->stack[offset]) ? '[' : '{');
        }
        fprintf(stderr, "\n");

}
jsonpg_parser jsonpg_new(jsonpg_config *config)
{
        jsonpg_config c = config_select(config); 

        size_t struct_bytes = sizeof(struct jsonpg_parser_s);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (c.stack_size + 7) / 8;
        jsonpg_parser p = jsonpg_alloc(struct_bytes + stack_bytes);
        if(p) {
                p->write_buf = str_buf_new();
                if(!p->write_buf) {
                        jsonpg_dealloc(p);
                        return NULL;
                }
                p->token_ptr = -1;
                p->stack_size = c.stack_size;
                p->stack = (uint8_t *)(((void *)p) + struct_bytes);
                p->flags = c.flags;
                p->reader = NULL;
                p->input_is_ours = 0;
                p->input = NULL;

                if(c.flags & JSONPG_FLAG_IS_OBJECT) {
                        p->stack_ptr = 0;
                        push_stack(STACK_OBJECT);
                        p->stack_ptr_min = 1;
                } else if(c.flags & JSONPG_FLAG_IS_ARRAY) {
                        p->stack_ptr = 0;
                        push_stack(STACK_ARRAY);
                        p->stack_ptr_min = 1;
                } else {
                        p->stack_top = -1;
                        p->stack_ptr_min = 0;
                }
        }
        return p;
}

void jsonpg_free(jsonpg_parser p) 
{
        if(p) {
                jsonpg_dealloc(p->input);
                str_buf_free(p->write_buf);
                jsonpg_dealloc(p);
        }
}

static jsonpg_type parse(
                jsonn_parser p,
                jsonn_generator g)
{
        jsonpg_type type;
        int abort = 0;
        while(!abort && JSONPG_EOF != (type = parse_next(p))) {
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
        p->input = p->current = p->write = json;
        p->input_size = length;
        p->input_is_ours = 0;
        p->last = json + length;
        p->seen_eof = 1;
        p->stack_ptr = p->stack_ptr_min;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return g
                ? parse(p, g)
                : JSONPG_ROOT;
}

jsonpq_type jsonpg_parse_str(
                jsonpg_parser p,
                char *json,
                jsonpg_generator g)
{
        return jsonpg_parse(p, (uint8_t *)json, strlen(s), g);
}

jsonpg_type jsonpg_parse_reader(
                jsonpg_parser p,
                jsonnpg_reader r,
                jsonnpg_generator g)
{
        if(!(p->input_is_ours && p->input)) {
                p->input = jsonpg_alloc(JSONPG_BUF_SIZE);
                if(!p->input)
                        return alloc_error(p);
                p->input_size = JSONPG_BUF_SIZE;
                p->input_is_ours = 1;
        }
        p->reader = r;
        int l = input_read(p, p->input);
        if(l < 0)
                return file_read_error(p);
        p->seen_eof = (0 == l);
        p->stack_ptr = p->stack_ptr_min;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return g
                ? parse(p, g)
                : JSONPG_ROOT;
}

size_t read_fd(jsonpg_reader r, void *buf, size_t count)
{
        return read(CTX_TO_INT(r->ctx), buf, count);
}

jsonpg_reader jsonpg_file_reader(int fd)
{
        jsonpg_reader r = jsonpg_alloc(sizeof(struct jsonpg_reader_s));
        if(!r)
                return NULL;
        r->read = read_fd;
        r->ctx = INT_TO_CTX(fd);
}

jsonpg_type jsonpg_parse_fd(
                jsonpg_parser p, 
                int fd, 
                jsonpg_generator g)
{
        p->reader = jsonpg_file_reader(fd);
        if(!p->reader)
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

void jsonpg_parse_start(
                jsonpg_parser p,
                uint8_t *json,
                size_t length)
{
        jsonpg_parse(p, json, length, NULL);
}

jsonpg_type jsonpg_parse_next(jsonpg_parser p)
{
        return jsonpg_next(p);
}

jsonpg_value jsonpg_parse_result(jsonpg_parser p)
{
        return p->result;
}



#define p_write(X, Y, Z)  (str_buf_append((X)->write_buf, (Y), (Z))
#define p_write_c(X, Y)   (str_buf_append_c((X)->write_buf, (Y))
#define p_content(X, Y)   (str_buf_content((X)->write_buf, (Y))

#define CODEPOINT_MAX 0x10FFFF

// codepoint breakpoints for encoding
#define _1_BYTE_MAX 0x7F
#define _2_BYTE_MAX 0x7FF
#define _3_BYTE_MAX 0xFFFF

// utf lead byte structure
#define CONTINUATION_BYTE  0x80
#define _2_BYTE_LEADER     0xC0
#define _3_BYTE_LEADER     0xE0
#define _4_BYTE_LEADER     0xF0

// bits masks
#define LO_3_BITS(x)  ((x) & 0x07)
#define LO_4_BITS(x)  ((x) & 0x0F)
#define LO_5_BITS(x)  ((x) & 0x1F)
#define LO_6_BITS(x)  ((x) & 0x3F)

// surrogate pair decoding
#define SURROGATE_OFFSET        0x10000
#define SURROGATE_LO_BITS(x)    ((x) & 0x03FF)


// Macros for code produced by gen_state
#define if_config(X)            (p->flag & (X))
#define accept_null(X)          ((X), JSONPG_NULL)
#define accept_true(X)          ((X), JSONPG_TRUE)
#define accept_false(X)         ((X), JSONPG_FALSE)
#define push_token(X)           p_push_token(p, (X))
#define pop_token()             p_pop_token(p)
#define ifpeek_token(X)         ((X) == p->tokens[p->token_ptr].type)
#define swap_token(X)           (p->tokens[p->token_ptr].type = (X))
#define push_state(X)           (p->push_state = (X))
#define pop_state()             (p->push_state)
#define begin_object()          p_begin_object(p)
#define end_object()            p_end_object(p)
#define begin_array()           p_begin_array(p)
#define end_array()             p_end_array(p)
#define in_object()             (p->stack_top == STACK_OBJECT)
#define in_array()              (p->stack_top == STACK_ARRAY)
#define accept_integer(X)       p_accept_integer(p, (X))
#define accept_real(X)          p_accept_real(p, (X))
#define accept_string(X)        p_accept_string(p, (X))
#define accept_key(X)           p_accept_key(p, (X))
#define accept_sq_string(X)     p_accept_string(p, (X))
#define accept_sq_key(X)        p_accept_key(p, (X))
#define accept_nq_string(X)     p_accept_string(p, (X))
#define accept_nq_key(X)        p_accept_key(p, (X))
#define process_escape(X)       p_process_escape(p, (X))
#define process_escape_chars(X) p_process_escape_chars(p, (X))
#define process_escape_u(X)     p_process_escape_u(p, (X))



#define TOKEN_INFO_DEFAULT      0x00
#define TOKEN_INFO_IS_STRING    0x01
#define TOKEN_INFO_HAS_QUOTE    0x03 // implies _IS_STRING
#define TOKEN_INFO_IS_ESCAPE    0x04
#define TOKEN_INFO_IS_SURROGATE 0x08
#define TOKEN_INFO_COPY_FORWARD 0x10

// must be same size and order as token_type
static int token_type_info[] {
        TOKEN_INFO_DEFAULT
        TOKEN_INFO_DEFAULT
        TOKEN_INFO_DEFAULT
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
        TOKEN_INFO_IS_SURROGAGE | TOKEN_INFO_COPY_FORWARD
};

typedef enum {
        config_comments = JSONPG_FLAG_COMMENTS,
        config_trailing_commas = JSONPG_FLAG_TRAILING_COMMAS,
        config_single_quotes = JSONPG_FLAG_CONFIG_SINGLE_QUOTES;
        config_unquoted_keys = JSONPG_FLAG_UNQUOTED_KEYS;
        config_unquoted_strings = JSONPG_FLAG_UNQUOTED_STRINGS;
        config_escape_characters = JSONPG_FLAG_ESCAPE_CHARACTERS;
        config_optional_commas = JSONPG_FLAG_OPTIONAL_COMMAS;
} config_flags;


static void p_push_token(jsonpg_parser p, token_type type)
{
        assert(p->token_ptr < p->token_ptr_max && "Token stack overflow");

        p->token_ptr++;
        token t = &p->tokens[p->token_ptr];
        t->type = type;
        t->pos = p->current;

        if(token_type_info[type] & TOKEN_INFO_HAS_QUOTE ) {
                // Quoted string/key starts after quote
                t->pos++;
        } else if(token_type_info[type] & TOKEN_INFO_IS_ESCAPE) {
                // Copy previous bytes from enclosing string
                assert(p->token_ptr > 0 && "Push escape token with no enclosing string");

                uint8_t *start = p->tokens[p->token_ptr - 1].pos;
                p_write(p, start, p->current - start);

                // Escape starts after "\"
                t->pos++;
        }
}

static token p_pop_token(jsonpg_parser p)
{
        assert(p->token_ptr >= 0 && "Token stack underflow");
        
        return &p->tokens[p->token_ptr--];
}


static int pop_stack(jsonpg_parser p) 
{
        if(p->stack_ptr <= p->stack_ptr_min)
                return 0;
        size_t sp = --p->stack_ptr;
        // pop state off stack
        p->stack_top = 0x01 & p->stack[sp >> 3] >> (sp & 0x07);

        return 1;
}

static int push_stack(jsonpg_parser p, stack_type type) 
{

        size_t sp = p->stack_ptr;
        if(sp >= p->stack_size) 
                return 0;
        int offset = sp >> 3;
        int mask = 1 << (sp & 0x07);
        
        if(type == STACK_ARRAY)
                p->stack[offset] |= mask;
        else 
                p->stack[offset] &= ~mask;
        p->stack_ptr++;
        p->stack_top = type;
        return 1;
}

static void p_begin_object(jsonpg_parser p)
{
        return(push_stack(p, STACK_OBJECT))
                ? JSONPG_BEGIN_OBJECT
                : jsonpg_error(p, JSG_ERROR_STACKOVERFLOW);
}

static void p_end_object(jsonpg_parser p)
{
        return (pop_stack(p))
                ? JSONPG_END_OBJECT
                : jsonpg_error(p, JSONPG_ERROR_STACKUNDERFLOW);
}

static void p_begin_array(jsonpg_parser p)
{
        return(push_stack(p, STACK_ARRAY))
                ? JSONPG_BEGIN_ARRAY
                : jsonpg_error(p, JSG_ERROR_STACKOVERFLOW);
}

static void p_end_array(jsonpg_parser p)
{
        return (pop_stack(p))
                ? JSONPG_END_ARRAY
                : jsonpg_error(p, JSONPG_ERROR_STACKUNDERFLOW);
}

static jsonpg_type p_accept_integer(jsonpg_parser p, token t)
{
        errno = 0;
        long integer = strtol((char *)t->pos, NULL, 10);
        if(errno) return jsonpg_number_error(p, token);
        p->result.number.integer = integer;
        return JSONPG_INTEGER;
}

static jsonpg_type p_accept_real(jsonpg_parser p, token t)
{
        errno = 0;
        long double ldreal = strtold((char *)t->pos, NULL);
        if(errno) return jsonpg_number_error(p, token);

        // precision problems
        // if we can go long double -> double -> long double
        // without losing information then we can store
        // the number in a double without loss of precision
        double real = (double)ldreal;
        if(ldreal != real)
                return jsonpg_number_error(p, token);

        p->result.number.real = real;
        return JSONPG_REAL;
}

static void set_string_value(jsonpg_parser p, token t)
{
        if(p->write_buf->count) {
                p_write(p, t->pos, p->current - t->pos);
                p->result.string.count = p_content(p, &p->result.string.bytes);
        } else {
                p->result.string.bytes = t->pos;
                p->result.string.count = p->current - t->pos;
        }
}

static jsonpg_type p_accept_string(jsonpg_parser p, token t)
{
        set_string_value(p, t);
        return JSONPG_STRING;
}

static jsonpg_type p_accept_key(jsonpg_parser p, token t)
{
        set_string_value(p, t);
        return JSONPG_KEY;
}

static void reset_string_after_escape(jsonpg_parser p)
{
        // After escape set enclosing string token.pos
        // to point to after the escape sequence
        assert(p->token_ptr >= 0 && "Pop escape token with no enclosing string");
        
        p->tokens[p->token_ptr].pos = 1 + p->current;
}

static void p_process_escape(jsonpg_parser p, token t)
{
        // Only JSON specified escape chars get here
        static char escapes = "bfnrt\"\\/";
        char *c = strchr(escapes, *t->pos);

        assert(c && "Invalid escape character");

        p_write_c(p, "\b\f\n\r\t\"\\/"[c - escapes]);

        reset_string_after_escape(p);
}

static void p_process_escape_chars(jsonpg_parser p, token t)
{
        p_write_c(p, *t->pos);
        reset_string_after_escape(p);
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

static int jsonpg_combine_surrogate_pair(int u1, int u2)
{
        return SURROGATE_OFFSET 
                + (SURROGATE_LO_BITS(u1) << 10) 
                + SURROGATE_LO_BITS(u2);
}

static int p_write_utf8(jsonpg_parser p, int cp) 
{
        int shift = 0;
        if(cp <= _1_BYTE_MAX) {
                // Ascii, just one byte
                p_write_c(p, cp);
        } else if(cp <= _2_BYTE_MAX) {
                // 2 byte UTF8, byte 1 is 110 and highest 5 bits
                shift = 6;
                p_write_c(p, _2_BYTE_LEADER | LO_5_BITS(cp >> shift));
        } else if(is_surrogate(cp)) {
                // UTF-16 surrogates are not legal Unicode
                // Parser should never let this happen so log it
                assert(0 && "Unicode codepoint in surrogate range");
        } else if(cp <= _3_BYTE_MAX) {
                // 3 byte UTF8, byte 1 is 1110 and highest 4 bits
                shift = 12;
                p_write_c(p, _3_BYTE_LEADER | LO_4_BITS(cp >> shift));
        } else if(cp <= CODEPOINT_MAX) {
                // 4 byte UTF8, byte 1 is 11110 and highest 3 bytes
                shift = 18;
                p_write_c(p, _4_BYTE_LEADER | LO_3_BITS(cp >> shift));
        } else {
                // value to large to be legal Unicode
                // Parser should never let this happen
                assert(0 && "Unicode codepoint > 0x10FFF");
        }
        // Now any continuation bytes
        // high two bits '10' and next highest 6 bits from codepoint 
        while(shift > 0) {
                shift -= 6;
                p_write_c(p, CONTINUATION_BYTE | LO_6_BITS(cp >> shift));
        }
        return 1;
}

static void p_process_escape_u(jsonpg_parser p, token t)
{
        // \uXXXX or \uXXXX\uXXXX
        // token points to first "u"
        int cp = parse_4hex(1 + t->pos);
        if(p->current - t->pos > 9)
                cp = jsonpg_combine_surrogate_pair(cp, parse_4hex(7 + t->pos));
        p_write_utf8(p, cp);

        reset_string_after_escape(p);
}

static void input_read(jsonpg_parser p, uint8_t *start)
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
        uint8_t start = p->input;
        if(p->token_ptr >= 0) {
                // We have a token on the stack
                // Perform any special processing required
                // to survive the crossing of input buffer boundaries
                token t = &p->tokens[p->token_ptr];
                int tinfo = token_info[t->type];
                if(tinfo & TOKEN_INFO_COPY_FORWARD) {
                        // copy bytes from current input needed
                        // for parsing next input
                        uint8_t *tpos;
                        if(tinfo & TOKEN_INFO_SURROGATE) {
                                // sub-token of escape_u
                                // and we need to copy entire escape sequence
                                assert(p->token_ptr > 0 
                                                && "Surrogate token without parent");
                                tpos = p->tokens[t->token_ptr - 1].pos;
                        } else {
                                tpos = t->pos;
                        }
                        while(tpos < p->last)
                                *start++ = *tpos++;
                        
                } else if(tinfo & TOKEN_INFO_IS_STRING) {
                        // copy string bytes as we are still in a string
                        p_write(p, t->pos, p->last - t->pos);
                }
        }
        int l = input_read(p, start);
        if(l >= 0)
                p->seen_eof = (l == 0);

        return l;
}
