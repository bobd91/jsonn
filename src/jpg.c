
// TODO function naming is a mess!
#ifndef JPG_STACK_SIZE
#define JPG_STACK_SIZE 1024
#endif

typedef enum {
        JPG_ROOT,
        JPG_FALSE,
        JPG_NULL,
        JPG_TRUE,
        JPG_INTEGER,
        JPG_REAL,
        JPG_STRING,
        JPG_STRING_NEXT,
        JPG_KEY,
        JPG_KEY_NEXT,
        JPG_BEGIN_ARRAY,
        JPG_END_ARRAY,
        JPG_BEGIN_OBJECT,
        JPG_END_OBJECT,
        JPG_OPTIONAL,
        JPG_ERROR,
        JPG_EOF
} jpg_type;

typedef enum {
        JPG_ERROR_NONE = 0,
        JPG_ERROR_CONFIG,
        JPG_ERROR_ALLOC,
        JPG_ERROR_PARSE,
        JPG_ERROR_NUMBER,
        JPG_ERROR_UTF8,
        JPG_ERROR_STACKUNDERFLOW,
        JPG_ERROR_STACKOVERFLOW,
        JPG_ERROR_FILE_READ
} jpg_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jpg_string_val;

typedef union {
        uint64_t integer;
        double real;
} jpg_number_val;

typedef struct {
        jpg_error_code code;
        size_t at;
} jpg_error_val;

typedef union {
        jpg_number_val number;
        jpg_string_val string;
        jpg_error_val error;
} jpg_value;

struct jpg_parser_s;
typedef struct jpg_parser_s *jpg_parser;

typedef struct {
        size_t stack_size;
        int16_t flags;
} jpg_config;

typedef struct jpg_node_s *jpg_node;
struct jpg_node_s {
        jpg_value is;
        jpg_type type;
};

typedef struct {
        int (*boolean)(void *ctx, int is_true);
        int (*null)(void *ctx);
        int (*integer)(void *ctx, int64_t integer);
        int (*real)(void *ctx, double real);
        int (*string)(void *ctx, uint8_t *bytes, size_t length);
        int (*key)(void *ctx, uint8_t *bytes , size_t length);
        int (*begin_array)(void *ctx);
        int (*end_array)(void *ctx);
        int (*begin_object)(void *ctx);
        int (*end_object)(void *ctx);
        int (*error)(void *ctx, jpg_error_code code, int at);
} jpg_callbacks;

typedef struct jpg_visitor_s {
        jpg_callbacks *callbacks;
        void *ctx;
} *jpg_visitor;

typedef struct jpg_print_ctx_s *jpg_print_ctx;

static void dump_p(jpg_parser p)
{
        fprintf(stderr, "Parser Error:\n");
        fprintf(stderr, "Error: %d\n", p->result.error.code);
        fprintf(stderr, "At Position: %ld\n", p->result.error.at);
        fprintf(stderr, "Input Length: %ld\n", p->last - p->start);
        fprintf(stderr, "Input Processed: %ld\n", p->current - p->start);
        fprintf(stderr, "Next State: %d\n", p->next);
        fprintf(stderr, "Stack Size: %ld\n", p->stack_size);
        fprintf(stderr, "Stack Pointer: %ld\n", p->stack_pointer);
        fprintf(stderr, "Stack: ");
        for(int i = 0 ; i < p->stack_pointer ; i++) {
                int offset = i >> 3;
                int mask = 1 << (i & 0x07);
                fprintf(stderr, "%c", 
                                (mask & p->stack[offset]) ? '[' : '{');
        }
        fprintf(stderr, "\n");

}

static jpg_type error(jpg_parser p, jpg_error_code code) 
{
        p->result.error.code = code;
        p->result.error.at = p->current - p->start;

        dump_p(p);

        return JPG_ERROR;
}

static jpg_type parse_error(jpg_parser p)
{
        return error(p, JPG_ERROR_PARSE);
}

static jpg_type number_error(jpg_parser p)
{
        return error(p, JPG_ERROR_NUMBER);
}

static jpg_type utf8_error(jpg_parser p)
{
        return error(p, JPG_ERROR_UTF8);
}

static jpg_type alloc_error(jpg_parser p)
{
        return error(p, JPG_ERROR_ALLOC);
}

static jpg_type file_read_error(jpg_parser p)
{
        return error(p, JPG_ERROR_FILE_READ);
}


void jpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

jpg_config jpg_config_get();
void jpg_config_set(jpg_config *config);

jpg_parser jpg_new(/* nullable */ jpg_config *config);
void jpg_free(jpg_parser p);

jpg_type jpg_parse(
                jpg_parser parser, 
                uint8_t *json, 
                size_t length,
                jpg_visitor visitor);

jpg_type jpg_parse_fd(
                jpg_parser parser,
                int fd,
                jpg_visitor visitor);

jpg_type jpg_parse_stream(
                jpg_parser parser,
                FILE *stream,
                jpg_visitor visitor);

void jpg_parse_start(jpg_parser parser, uint8_t *json, size_t length);
jpg_type jpg_parse_next(jpg_parser parser);

jpg_value jpg_parse_result(jpg_parser);

void jpg_set_allocators(
                void *(*malloc)(size_t),
                void *(*realloc)(void *, size_t),
                void (*free)(void *))
{
        jpg_alloc = malloc;
        jpg_realloc = realloc;
        jpg_dealloc = free;
}

jpg_parser jpg_new(jpg_config *config)
{
        jpg_config c = config_select(config); 

        size_t struct_bytes = sizeof(struct jpg_parser_s);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (c.stack_size + 7) / 8;
        jpg_parser p = jpg_alloc(struct_bytes + stack_bytes);
        if(p) {
                p->stack_size = c.stack_size;
                p->stack = (uint8_t *)(((void *)p) + struct_bytes);
                p->flags = c.flags;
                p->repeat_next = PARSE_NEXT;
        }
        return p;
}

void jpg_free(jpg_parser p) 
{
        if(p) jpg_dealloc(p);
}

static jpg_type parse_visit(
                jpg_parser p,
                jpg_visitor visitor)
{
        jpg_type type;
        int abort = 0;
        while(!abort && JPG_EOF != (type = jpg_next(p))) {
                abort = visit(visitor, type, &p->result);
        }

        return type;
}

jpg_type jpg_parse(
                jpg_parser p, 
                uint8_t *json, 
                size_t length,
                jpg_visitor visitor)
{
        p->next = jpg_init_next(p);
        p->start = p->current = p->write = json;
        p->last = json + length;
        p->seen_eof = 1;
        *p->last = '\0';
        p->stack_pointer = 0;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return visitor
                ? parse_visit(p, visitor)
                : JPG_ROOT;
}
static void *(*jsonn_alloc)(size_t) = malloc;
static void *(*jsonn_realloc)(void *, size_t) = realloc;
static void (*jsonn_dealloc)(void *) = free;

jpg_type jpg_parse_fd(jpg_parser p, int fd, jpg_visitor visitor)
{
        p->next = jpg_init_next(p);
        p->fd = fd;
        p->buffer_root = buffer_block_new();
        if(!p->buffer_root)
                return alloc_error(p);
        buffer buff = buffer_new(p->buffer_root);
        if(!buff)
                return alloc_error(p);
        int res = buffer_fill(p, buff);
        if(res == -1)
                return file_read_error(p);
        p->seen_eof = (0 == res);
        p->start = p->current = buff->start;
        p->last = buff->last;
        *p->last = '\0';
        p->stack_pointer = 0;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return visitor
                ? parse_visit(p, visitor)
                : JPG_ROOT;
}

jpg_type jpg_parse_stream(jpg_parser p, FILE *stream, jpg_visitor visitor)
{
        return jpg_parse_fd(p, fileno(stream), visitor);
}

void jpg_parse_start(
                jpg_parser p,
                uint8_t *json,
                size_t length)
{
        jpg_parse(p, json, length, NULL);
}

jpg_type jpg_parse_next(jpg_parser p)
{
        return jpg_next(p);
}

jpg_value jpg_parse_result(jpg_parser p)
{
        return p->result;
}

struct jpg_parser_s {
        uint8_t *start;   
        uint8_t *current;
        uint8_t *last;
        str_buf wbuf;
        uint16_t flags;
        jpg_value result;
        int seen_eof;
        size_t stack_size;
        size_t stack_pointer;
        uint8_t *stack;
};

typedef struct jpg_parser_s *jpg_parser;


#define p_write(X, Y, Z)  (str_buf_append((X)->wbuf, (Y), (Z))
#define p_write_c(X, Y)   (str_buf_append_c((X)->wbuf, (Y))
#define p_content(X, Y)   (str_buf_content((X)->wbuf, (Y))

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
#define accept_null(X)          ((X), JPG_NULL)
#define accept_true(X)          ((X), JPG_TRUE)
#define accept_false(X)         ((X), JPG_FALSE)
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
#define in_object()             (p->stack_top == JPG_STACK_OBJECT)
#define in_array()              (p->stack_top == JPG_STACK_ARRAY)
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

typedef enum {
        token_null,
        token_true,
        token_false,
        token_string,
        token_key,
        token_sq_string,
        token_sq_key,
        token_nq_string,
        token_nq_key,
        token_integer,
        token_real,
        token_escape,
        token_escape_chars,
        token_escape_u,
        token_surrogate
} token_type;

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
        config_comments = JPG_FLAG_COMMENTS,
        config_trailing_commas = JPG_FLAG_TRAILING_COMMAS,
        config_single_quotes = JPG_FLAG_CONFIG_SINGLE_QUOTES;
        config_unquoted_keys = JPG_FLAG_UNQUOTED_KEYS;
        config_unquoted_strings = JPG_FLAG_UNQUOTED_STRINGS;
        config_escape_characters = JPG_FLAG_ESCAPE_CHARACTERS;
        config_optional_commas = JPG_FLAG_OPTIONAL_COMMAS;
} config_flags;

typedef struct token_s {
        token_type type;
        uint8_t *pos;
} *token;

static void p_push_token(jpg_parser p, token_type type)
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

static token p_pop_token(jpg_parser p)
{
        assert(p->token_ptr >= 0 && "Token stack underflow");
        
        return &p->tokens[p->token_ptr--];
}


static int pop_stack(jpg_parser p) 
{
        if(p->stack_pointer == p->stack_pointer_min)
                return 0;
        size_t sp = --p->stack_pointer;
        // pop state off stack
        p->stack_top = 0x01 & p->stack[sp >> 3] >> (sp & 0x07);

        return 1;
}

static int push_stack(jpg_parser p, stack_type type) 
{

        size_t sp = p->stack_pointer;
        if(sp >= p->stack_size) 
                return 0;
        int offset = sp >> 3;
        int mask = 1 << (sp & 0x07);
        
        if(type == JPG_STACK_ARRAY)
                p->stack[offset] |= mask;
        else 
                p->stack[offset] &= ~mask;
        p->stack_pointer++;
        p->stack_top = type;
        return 1;
}

static void p_begin_object(jpg_parser p)
{
        return(push_stack(p, JPG_STACK_OBJECT))
                ? JPG_BEGIN_OBJECT
                : jpg_error(p, JSG_ERROR_STACKOVERFLOW);
}

static void p_end_object(jpg_parser p)
{
        return (pop_stack(p))
                ? JPG_END_OBJECT
                : jpg_error(p, JPG_ERROR_STACKUNDERFLOW);
}

static void p_begin_array(jpg_parser p)
{
        return(push_stack(p, JPG_STACK_ARRAY))
                ? JPG_BEGIN_ARRAY
                : jpg_error(p, JSG_ERROR_STACKOVERFLOW);
}

static void p_end_array(jpg_parser p)
{
        return (pop_stack(p))
                ? JPG_END_ARRAY
                : jpg_error(p, JPG_ERROR_STACKUNDERFLOW);
}

static jpg_type p_accept_integer(jpg_parser p, token t)
{
        errno = 0;
        long integer = strtol((char *)t->pos, NULL, 10);
        if(errno) return jpg_number_error(p, token);
        p->result.number.integer = integer;
        return JPG_INTEGER;
}

static jpg_type p_accept_real(jpg_parser p, token t)
{
        errno = 0;
        long double ldreal = strtold((char *)t->pos, NULL);
        if(errno) return jpg_number_error(p, token);

        // precision problems
        // if we can go long double -> double -> long double
        // without losing information then we can store
        // the number in a double without loss of precision
        double real = (double)ldreal;
        if(ldreal != real)
                return jpg_number_error(p, token);

        p->result.number.real = real;
        return JPG_REAL;
}

static void set_string_value(jpg_parser p, token t)
{
        if(p->wbuf->count) {
                p_write(p, t->pos, p->current - t->pos);
                p->result.string.count = p_content(p, &p->result.string.bytes);
        } else {
                p->result.string.bytes = t->pos;
                p->result.string.count = p->current - t->pos;
        }
}

static jpg_type p_accept_string(jpg_parser p, token t)
{
        set_string_value(p, t);
        return JPG_STRING;
}

static jpg_type p_accept_key(jpg_parser p, token t)
{
        set_string_value(p, t);
        return JPG_KEY;
}

static void reset_string_after_escape(jpg_parser p)
{
        // After escape set enclosing string token.pos
        // to point to after the escape sequence
        assert(p->token_ptr >= 0 && "Pop escape token with no enclosing string");
        
        p->tokens[p->token_ptr].pos = 1 + p->current;
}

static void p_process_escape(jpg_parser p, token t)
{
        // Only JSON specified escape chars get here
        static char escapes = "bfnrt\"\\/";
        char *c = strchr(escapes, *t->pos);

        assert(c && "Invalid escape character");

        p_write_c(p, "\b\f\n\r\t\"\\/"[c - escapes]);

        reset_string_after_escape(p);
}

static void p_process_escape_chars(jpg_parser p, token t)
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

static int jpg_combine_surrogate_pair(int u1, int u2)
{
        return SURROGATE_OFFSET 
                + (SURROGATE_LO_BITS(u1) << 10) 
                + SURROGATE_LO_BITS(u2);
}

static int p_write_utf8(jpg_parser p, int cp) 
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

static void p_process_escape_u(jpg_parser p, token t)
{
        // \uXXXX or \uXXXX\uXXXX
        // token points to first "u"
        int cp = parse_4hex(1 + t->pos);
        if(p->current - t->pos > 9)
                cp = jpg_combine_surrogate_pair(cp, parse_4hex(7 + t->pos));
        p_write_utf8(p, cp);

        reset_string_after_escape(p);
}

static void input_read(jpg_parser p, uint8_t *start)
{
       uint8_t *pos = start;
       int max = p->input_size - (start - p->input);
       while(max) {
               int l = p->source->read(p->source, pos, max);
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

static int parser_read_next(jpg_parser p)
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
        int l input_read(p, start);
        if(l >= 0)
                p->seen_eof = (l == 0);

        return l;
}
