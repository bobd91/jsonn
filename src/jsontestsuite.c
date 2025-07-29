#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "jsonn.c"
int do_boolean(jsonn_parser p, int is_true) 
{
        printf("%s,", is_true ? "true" : "false");
        return 0;
}

int do_null(jsonn_parser p) 
{
        printf("null,");
        return 0;
}

int do_long(jsonn_parser p, int64_t l) 
{
        printf("%ld,", l);
        return 0;
}

int do_double(jsonn_parser p, double d) 
{
        printf("%lf,", d);
        return 0;
}

int do_string(jsonn_parser p, jsonn_string *s)
{
        printf("\"%s\"", s->bytes);
        return 0;
}

int do_key(jsonn_parser p, jsonn_string *s) 
{
        printf("\"%s\":", s->bytes);
        return 0;
}

int do_begin_array(jsonn_parser p)
{
        printf("[");
        return 0;
}

int do_end_array(jsonn_parser p)
{
        printf("]");
        return 0;
}

int do_begin_object(jsonn_parser p) 
{
        printf("{");
        return 0;
}

int do_end_object(jsonn_parser p) 
{
        printf("}");
        return 0;
}

int do_error(jsonn_parser p, jsonn_error *e)
{
        printf("\nError: %d [%ld]", e->code, e->at);
        return 0;
}


jsonn_callbacks callbacks = {
        .j_boolean = do_boolean,
        .j_null = do_null,
        .j_long = do_long,
        .j_double = do_double,
        .j_string = do_string,
        .j_key = do_key,
        .j_begin_array = do_begin_array,
        .j_end_array = do_end_array,
        .j_begin_object = do_begin_object,
        .j_end_object = do_end_object,
        .j_error = do_error
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
                        jsonn_type t = jsonn_parse(p, buf, length, &callbacks);
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

                        



