

#define if_config(X) (p->flag & (X))
#define accept_null(X) ((X), JSONN_NULL)
#define accept_true(X) ((X), JSONN_TRUE)
#define accept_false(X) ((X), JSONN_FALSE)
#define push_token(X) (p_push_token(p, (X)))
#define pop_token() (p_pop_token(p))
#define ifpeek_token(X) ((X) == p->tokens[p->token_ptr].type)
#define swap_token(X) (p->tokens[p->token_ptr].type = (X))
#define push_state(X) (p->push_state = (X))
#define pop_state() (p->push_state)
#define begin_object() (p_begin_object(p))
#define end_object() (p_end_object(p))
#define begin_array() (p_begin_array(p))
#define end_array() (p_end_array(p))
#define in_object() (p_in_object(p))
#define in_array() (p_in_array(p))
#define accept_number(X) (p_accept_number(p, (X)))
#define accept_string(X) (p_accept_string(p, (X)))
#define accept_key(X) (p_accept_key(p, (X)))
#define process_escape(X) (p_process_escape(p, (X)))
#define process_escape_chars(X) (p_process_escape_chars(p, (X)))
#define process_escape_u(X) (p_process_escape_u(p, (X)))

typedef enum {
        token_null,
        token_true,
        token_false,
        token_string,
        token_key,
        token_number,
        token_double_quote,
        token_single_quote,
        token_escape,
        token_escape_chars,
        token_escape_u,
        token_surrogate
} token_type;

typedef enum {
        config_comments = JSONN_FLAG_COMMENTS,
        config_trailing_commas = JSONN_FLAG_TRAILING_COMMAS,
        config_single_quotes = JSONN_FLAG_CONFIG_SINGLE_QUOTES;
        config_unquoted_keys = JSONN_FLAG_UNQUOTED_KEYS;
        config_unquoted_strings = JSONN_FLAG_UNQUOTED_STRINGS;
        config_escape_characters = JSONN_FLAG_ESCAPE_CHARACTERS;
        config_optional_commas = JSONN_FLAG_OPTIONAL_COMMAS;
} config_flags;

typedef struct token_s {
        token_type type;
        uint8_t *pos;
} token;

static void p_push_token(jsonn_parser p, token_type type)
{
        assert(p->token_ptr < p->token_ptr_max);

        p->token_ptr++;
        p->tokens[p->token_ptr].type = type;
        p->tokens[p->token_ptr].pos = p->current;
}

static token p_pop_token(jsonn_parser p)
{
        assert(p->token_ptr >= 0);

        return p->tokens[p->token_ptr--];
}

static void p_begin_object(jsonn_parser p)
{
}

static void p_end_object(jsonn_parser p)
{
}

static void p_begin_array(jsonn_parser p)
{
}

static void p_end_array(jsonn_parser p)
{
}

static int p_in_object(jsonn_parser p)
{
}

static int p_in_array(jsonn_parser p)
{
}

static jsonn_type p_accept_number(jsonn_parser p, token t)
{
}

static jsonn_type p_accept_string(jsonn_parser p, token t)
{
}

static jsonn_type p_accept_key(jsonn_parser p, token t)
{
}

static void p_process_escape(jsonn_parser p, token t)
{
}

static void p_process_escape_chars(jsonn_parser p, token t)
{
}

static void p_process_escape_u(jsonn_parser p, token t)
{
}











        
        
        

