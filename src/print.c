#include <stdio.h>
#include <fcntl.h>

typedef struct jsonn_print_ctx_s {
        int fd;
        int level;
        int comma;
        int key;
        int pretty;
        int nl;
} jsonn_print_ctx;

typedef jsonn_print_ctx *print_ctx;

static void print_indent(print_ctx ctx)
{
        // Avoid leading newline
        if(ctx->nl)
                printf("\n");
        else
                ctx->nl = 1;

        for(int i = 0 ; i < ctx->level ; i++)
                printf("    ");
}

static void print_prefix(print_ctx ctx)
{
        if(!ctx->key) {
                if(ctx->comma)
                        printf(",");
                if(ctx->pretty)
                        print_indent(ctx);
                
        }
        ctx->comma = 1;
        ctx->key = 0;
}

static void print_begin_prefix(print_ctx ctx)
{
        print_prefix(ctx);
        ctx->comma = 0;
        ctx->level++;
}

static void print_end_prefix(print_ctx ctx)
{
        ctx->level--;
        if(ctx->comma) {
                ctx->comma = 0; // no trailing comma
                print_prefix(ctx);
        }
        ctx->comma = 1;
}

static void print_key_suffix(print_ctx ctx)
{
        printf(":");
        if(ctx->pretty)
                printf(" ");
        ctx->key = 1;
}

static int print_boolean(void *ctx, int is_true) 
{
        print_prefix(ctx);
        printf("%s", is_true ? "true" : "false");
        return 0;
}

static int print_null(void *ctx) 
{
        print_prefix(ctx);
        printf("null");
        return 0;
}

static int print_integer(void *ctx, int64_t l) 
{
        print_prefix(ctx);
        printf("%ld", l);
        return 0;
}

static int print_real(void *ctx, double d) 
{
        print_prefix(ctx);
        printf("%.16g", d);
        return 0;
}

static int print_string(void *ctx, uint8_t *bytes, size_t length, int complete)
{
        print_prefix(ctx);
        if(length) {
                char *fmt = complete
                        ? "\"%.*s\""
                        : "\"%.*s";
                printf(fmt, (int)length, bytes);
        }
        return 0;
}

static int print_string_next(void *ctx, uint8_t *bytes, size_t length, int complete)
{
        if(length) {
                char *fmt = complete
                        ? "%.*s\""
                        : "%.*s";
                printf(fmt, (int)length, bytes);
        }
        return 0;
}

static int print_key(void *ctx, uint8_t *bytes, size_t length, int complete) 
{
        print_prefix(ctx);
        if(length) {
                char *fmt = complete
                        ? "\"%.*s\""
                        : "\"%.*s";
                printf(fmt, (int)length, bytes);
                if(complete)
                        print_key_suffix(ctx);
        }
        return 0;
}

static int print_key_next(void *ctx, uint8_t *bytes, size_t length, int complete) 
{
        if(length) {
                char *fmt = complete
                        ? "%.*s\""
                        : "%.*s";
                printf(fmt, (int)length, bytes);
                if(complete)
                        print_key_suffix(ctx);
        }
        return 0;
}

static int print_begin_array(void *ctx)
{
        print_begin_prefix(ctx);
        printf("[");
        return 0;
}

static int print_end_array(void *ctx)
{
        print_end_prefix(ctx);
        printf("]");
        return 0;
}

static int print_begin_object(void *ctx) 
{
        print_begin_prefix(ctx);
        printf("{");
        return 0;
}

static int print_end_object(void *ctx) 
{
        print_end_prefix(ctx);
        printf("}");
        return 0;
}

static int print_error(void *ctx, jsonn_error_code code, int at)
{
        printf("\nError: %d [%d]", code, at);
        return 0;
}


static jsonn_callbacks printer_callbacks = {
        .boolean = print_boolean,
        .null = print_null,
        .integer = print_integer,
        .real = print_real,
        .string = print_string,
        .string_next = print_string_next,
        .key = print_key,
        .key_next = print_key_next,
        .begin_array = print_begin_array,
        .end_array = print_end_array,
        .begin_object = print_begin_object,
        .end_object = print_end_object,
        .error = print_error
};

jsonn_visitor jsonn_file_printer(int fd, int pretty, jsonn_print_ctx *ctx)
{
        ctx->fd = fd;
        ctx->pretty = pretty;
        jsonn_visitor v = {
                .callbacks = &printer_callbacks,
                .ctx = ctx
        };
        return v;
}

jsonn_visitor jsonn_stream_printer(FILE *stream, int pretty, jsonn_print_ctx *ctx)
{
        return jsonn_file_printer(fileno(stream), pretty, ctx);
}



