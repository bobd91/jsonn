#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "jsonn.c"

static int do_boolean(void *ctx, int is_true) 
{
        printf("%s,", is_true ? "true" : "false");
        return 0;
}

static int do_null(void *ctx) 
{
        printf("null,");
        return 0;
}

static int do_integer(void *ctx, int64_t l) 
{
        printf("%ld,", l);
        return 0;
}

static int do_real(void *ctx, double d) 
{
        printf("%lf,", d);
        return 0;
}

static int do_string(void *ctx, uint8_t *bytes, size_t length)
{
        printf("\"%.*s", length, bytes);
        return 0;
}

static int do_key(void *ctx, uint8_t *bytes, size_t length) 
{
        printf("\"%.*s", length, bytes);
        return 0;
}

static int do_begin_array(void *ctx)
{
        printf("[");
        return 0;
}

static int do_end_array(void *ctx)
{
        printf("]");
        return 0;
}

static int do_begin_object(void *ctx) 
{
        printf("{");
        return 0;
}

static int do_end_object(void *ctx) 
{
        printf("}");
        return 0;
}

static int do_error(void *ctx, jsonn_error_code code, int at)
{
        printf("\nError: %d [%ld]", code, at);
        return 0;
}


static jsonn_callbacks callbacks = {
        .boolean = do_boolean,
        .null = do_null,
        .integer = do_integer,
        .real = do_real,
        .string = do_string,
        .key = do_key,
        .begin_array = do_begin_array,
        .end_array = do_end_array,
        .begin_object = do_begin_object,
        .end_object = do_end_object,
        .error = do_error
};

static jsonn_visitor visitor = {
        .callbacks = &callbacks,
};

int main(int argc, char *argv[]) {
        if(2 != argc) 
                return 2;

        jsonn_parser p = jsonn_new(NULL);

        FILE *fh = fopen(argv[1], "rb");
        if(fh) {
                fseek(fh, 0L, SEEK_END);
                long length = ftell(fh);
                rewind(fh);
                uint8_t *buf = malloc(length + 1);
                if(buf) {
                        fread(buf, length, 1, fh);
                        jsonn_type t = jsonn_parse(p, buf, length, &visitor);
                        jsonn_free(p);
                        free(buf);
                        int ret = (t == JSONN_EOF) ? 0 : 1;
                        printf("Type: %d, Returned %d\n", t, ret);
                        return ret;
                }
                fclose(fh);
        }
        return 2;
}

                        



