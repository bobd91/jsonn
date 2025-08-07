#include <stdio.h>
#include <fcntl.h>
#include <math.h>

// TODO error handling of writer errors

#define INT_FROM_VOIDPTR(X) ((int)(int64_t)X)
#define INT_TO_VOIDPTR(X)   ((void *)(int64_t)X)

char number_buffer[32];

typedef struct jsonn_print_ctx_s *jsonn_print_ctx;

struct jsonn_print_ctx_s {
        int level;
        int comma;
        int key;
        int pretty;
        int nl;
        int (*write)(void *, uint8_t *, size_t, int);
        void *writer;
};

static int write_c(jsonn_print_ctx ctx, char c)
{
        return ctx->write(ctx->writer, (uint8_t *)&c, 1, 0);
}

static int write_s(jsonn_print_ctx ctx, char *s)
{
        return ctx->write(ctx->writer, (uint8_t *)s, strlen(s), 0);
}

static int write_utf8(jsonn_print_ctx ctx, uint8_t *bytes, size_t count) 
{
        return ctx->write(ctx->writer, bytes, count, 1);
}

static void print_indent(jsonn_print_ctx ctx)
{
        // Avoid leading newline
        if(ctx->nl)
                write_c(ctx, '\n');
        else
                ctx->nl = 1;

        for(int i = 0 ; i < ctx->level ; i++)
                write_s(ctx, "    ");
}

static void print_prefix(jsonn_print_ctx ctx)
{
        if(!ctx->key) {
                if(ctx->comma)
                        write_c(ctx, ',');
                if(ctx->pretty)
                        print_indent(ctx);
                
        }
        ctx->comma = 1;
        ctx->key = 0;
}

static void print_begin_prefix(jsonn_print_ctx ctx)
{
        print_prefix(ctx);
        ctx->comma = 0;
        ctx->level++;
}

static void print_end_prefix(jsonn_print_ctx ctx)
{
        ctx->level--;
        if(ctx->comma) {
                ctx->comma = 0; // no trailing comma
                print_prefix(ctx);
        }
        ctx->comma = 1;
}

static void print_key_suffix(jsonn_print_ctx ctx)
{
        write_c(ctx, ':');
        if(ctx->pretty)
                write_c(ctx, ' ');
        ctx->key = 1;
}

static int print_boolean(void *ctx, int is_true) 
{
        print_prefix(ctx);
        write_s(ctx, is_true ? "true" : "false");
        return 0;
}

static int print_null(void *ctx) 
{
        print_prefix(ctx);
        write_s(ctx, "null");
        return 0;
}

static int print_integer(void *ctx, int64_t l) 
{
        print_prefix(ctx);
        int r = snprintf(number_buffer, sizeof(number_buffer), "%ld", l);
        if(r < 0) {
                // TODO better errors
                return 1;
        }

        write_s(ctx, number_buffer);
        return 0;
}

static int print_real(void *ctx, double d) 
{
        if(!isnormal(d)) {
                // TODO better errors
                return 1;
        }
        print_prefix(ctx);
        int r = snprintf(number_buffer, sizeof(number_buffer), "%16g", d);
        if(r < 0) {
                // TODO better errors
                return 1;
        }

        // snprintf(%16g) writes at the bak of the buffer
        // so we get lots of leading spaces
        char *s = number_buffer;
        while(*s == ' ')
                s++;
        write_s(ctx, s);

        // real number without decimal point or exponent
        // add .0 at the end to preserve type at next JSON decode
        if(r == strcspn(number_buffer, ".e"))
                write_s(ctx, ".0");

        return 0;
}

static int print_string(void *ctx, uint8_t *bytes, size_t length)
{
        print_prefix(ctx);
        write_utf8(ctx, bytes, length); 
        return 0;
}

static int print_key(void *ctx, uint8_t *bytes, size_t length) 
{
        print_prefix(ctx);
        write_utf8(ctx, bytes, length);
        print_key_suffix(ctx);  
        return 0;
}

static int print_begin_array(void *ctx)
{
        print_begin_prefix(ctx);
        write_c(ctx, '[');
        return 0;
}

static int print_end_array(void *ctx)
{
        print_end_prefix(ctx);
        write_c(ctx, ']');
        return 0;
}

static int print_begin_object(void *ctx) 
{
        print_begin_prefix(ctx);
        write_c(ctx, '{');
        return 0;
}

static int print_end_object(void *ctx) 
{
        print_end_prefix(ctx);
        write_c(ctx, '}');
        return 0;
}

static int print_error(void *ctx, jsonn_error_code code, int at)
{
        fprintf(stderr, "\nError: %d [%d]", code, at);
        return 1;
}


static jsonn_callbacks printer_callbacks = {
        .boolean = print_boolean,
        .null = print_null,
        .integer = print_integer,
        .real = print_real,
        .string = print_string,
        .key = print_key,
        .begin_array = print_begin_array,
        .end_array = print_end_array,
        .begin_object = print_begin_object,
        .end_object = print_end_object,
        .error = print_error
};


int write_file(void *ctx, uint8_t *bytes, size_t count, int encode_utf8)
{
        // stop compiler warning
        // we know what we are doing ... lol
        int fd = INT_FROM_VOIDPTR(ctx);
        uint8_t *start = bytes;
        size_t size = count;
        while(size) {
                size_t w = write(fd, start, size);
                if(w < 0) {
                        // TODO errors
                        return (int)w;
                }
                start += w;
                size -= w;
        }
        return count;
}

jsonn_visitor jsonn_file_printer(int fd, int pretty)
{
        jsonn_visitor v = jsonn_visitor_new(
                        &printer_callbacks, 
                        sizeof(struct jsonn_print_ctx_s));
        if(!v)
                return NULL;

        jsonn_print_ctx ctx = v->ctx;
        ctx->write = write_file;
        ctx->writer = INT_TO_VOIDPTR(fd);
        ctx->pretty = pretty;

        return v;
}

jsonn_visitor jsonn_stream_printer(FILE *stream, int pretty)
{
        return jsonn_file_printer(fileno(stream), pretty);
}



