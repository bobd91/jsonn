
#ifndef JSONPG_STACK_SIZE
#define JSONPG_STACK_SIZE 1024
#endif

#define JSONPG_FLAG_COMMENTS                   0x01
#define JSONPG_FLAG_TRAILING_COMMAS            0x02
#define JSONPG_FLAG_SINGLE_QUOTES              0x04
#define JSONPG_FLAG_UNQUOTED_KEYS              0x08
#define JSONPG_FLAG_UNQUOTED_STRINGS           0x10
#define JSONPG_FLAG_ESCAPE_CHARACTERS          0x20
#define JSONPG_FLAG_OPTIONAL_COMMAS            0x40
#define JSONPG_FLAG_IS_OBJECT                  0x80
#define JSONPG_FLAG_IS_ARRAY                   0x100

typedef enum {
        JSONPG_ROOT,
        JSONPG_FALSE,
        JSONPG_NULL,
        JSONPG_TRUE,
        JSONPG_INTEGER,
        JSONPG_REAL,
        JSONPG_STRING,
        JSONPG_STRING_NEXT,
        JSONPG_KEY,
        JSONPG_KEY_NEXT,
        JSONPG_BEGIN_ARRAY,
        JSONPG_END_ARRAY,
        JSONPG_BEGIN_OBJECT,
        JSONPG_END_OBJECT,
        JSONPG_OPTIONAL,
        JSONPG_ERROR,
        JSONPG_EOF
} jsonpg_type;

typedef enum {
        JSONPG_ERROR_NONE = 0,
        JSONPG_ERROR_CONFIG,
        JSONPG_ERROR_ALLOC,
        JSONPG_ERROR_PARSE,
        JSONPG_ERROR_NUMBER,
        JSONPG_ERROR_UTF8,
        JSONPG_ERROR_STACKUNDERFLOW,
        JSONPG_ERROR_STACKOVERFLOW,
        JSONPG_ERROR_FILE_READ
} jsonpg_error_code;

typedef struct {
        uint8_t *bytes;
        size_t length;
} jsonpg_string_val;

typedef union {
        uint64_t integer;
        double real;
} jsonpg_number_val;

typedef struct {
        jsonpg_error_code code;
        size_t at;
} jsonpg_error_val;

typedef union {
        jsonpg_number_val number;
        jsonpg_string_val string;
        jsonpg_error_val error;
} jsonpg_value;

typedef struct {
        size_t stack_size;
        int16_t flags;
} jsonpg_config;

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
        int (*error)(void *ctx, jsonpg_error_code code, int at);
} jsonpg_callbacks;

typedef struct jsonpg_generator_s {
        jsonpg_callbacks *callbacks;
        void *ctx;
} *jsonpg_generator;

typedef struct jsonpg_reader_s *jsonpg_reader;

struct jsonpg_reader_s {
        int (*read)(jsonpg_reader, void *, uint32_t);
        void *ctx;
};

typedef struct jsonpg_print_ctx_s *jsonpg_print_ctx;

struct jsonpg_parser_s;
typedef struct jsonpg_parser_s *jsonpg_parser;

void jsonpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

jsonpg_config jsonpg_config_get();
void jsonpg_config_set(jsonpg_config *);

jsonpg_parser jsonpg_new(/* nullable */ jsonpg_config *);
void jsonpg_free(jsonpg_parser);

jsonpg_type jsonpg_parse(
                jsonpg_parser, 
                uint8_t *, 
                uint32_t,
                jsonpg_generator);

jsonpg_type jsonpg_parse_str(
                jsonpg_parser,
                char *,
                jsonpg_generator);

jsonpg_type jsonpg_parse_fd(
                jsonpg_parser,
                int,
                jsonpg_generator);

jsonpg_type jsonpg_parse_stream(
                jsonpg_parser,
                FILE *,
                jsonpg_generator);

jsonpg_type jsonpg_parse_reader(
                jsonpg_parser,
                jsonpg_reader,
                jsonpg_generator);

jsonpg_type jsonpg_next(jsonpg_parser);

jsonpg_value jsonpg_result(jsonpg_parser);

