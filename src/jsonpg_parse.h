
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

typedef struct token_s {
        token_type type;
        uint8_t *pos;
} *token;

#define JSONPG_TOKEN_MAX 3 // string/escape_u/surrogate

struct str_buf_s;
typedef struct str_buf_s *str_buf;

struct jsonpg_parser_s {
        uint8_t seen_eof;
        uint8_t token_ptr;
        uint8_t stack_top;
        uint8_t stack_ptr_min;
        uint8_t input_is_ours;
        uint16_t flags;
        uint16_t stack_size;
        uint16_t stack_ptr;
        uint32_t input_size;
        uint8_t *input;   
        uint8_t *current;
        uint8_t *last;
        str_buf write_buf;
        uint8_t *stack;
        jsonpg_value result;
        struct token_s[JSONPG_TOKEN_MAX];
};

typedef struct jsonpg_parser_s *jsonpg_parser;

