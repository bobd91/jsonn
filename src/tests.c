#include <stdio.h>
#include "jsonn.c"

int printp(jsonn_parser p) 
{
        uint8_t *s = p->start;
        while(s <= p->last) {
                printf("%c", (*s != '\0') ? *s : '^');
                s++;
        }
        puts("");
        s = p->start;
        while(s <= p->last) {
                printf("%c", (s == p->current) ? 'C' : ' ');
                s++;
        }
        puts("");
        s = p->start;
        while(s <= p->last) {
                printf("%c", (s == p->write) ? 'W' : ' ');
                s++;
        }
        puts("");
        return 0;
}

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
        printf("\"%.*s\"", length, bytes);
        return 0;
}

static int do_key(void *ctx, uint8_t *bytes, size_t length) 
{
        printf("\"%.*s\":", length, bytes);
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
        .ctx = NULL
};

jsonn_parser pp;

int main(int argc, char *argv[])
{
        jsonn_config c = jsonn_config_get();
        c.flags |=   JSONN_FLAG_COMMENTS
                   | JSONN_FLAG_TRAILING_COMMAS
                   | JSONN_FLAG_REPLACE_ILLFORMED_UTF8
                   | JSONN_FLAG_OPTIONAL_COMMAS
                   | JSONN_FLAG_UNQUOTED_KEYS
                   | JSONN_FLAG_UNQUOTED_STRINGS
                   | JSONN_FLAG_ESCAPE_CHARACTERS
                   | JSONN_FLAG_IS_ARRAY
                   ;

        jsonn_parser p = jsonn_new(NULL); //&c);

        puts(argv[1]);
        pp = p;

        uint8_t buf[1024];
        char *json = argv[1];
        size_t len = strlen(json);
        memcpy(buf, json, len + 1);
        jsonn_type res = jsonn_parse(p, buf, len, &visitor);
        if(res == JSONN_EOF)
                printf("\n\nResult EOF: %d\n", res);
        else
                printf("\n\nResult : %d (%d[%ld])\n", res, p->result.error.code, p->result.error.at);

        jsonn_free(p);


       
}


