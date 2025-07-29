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

int do_boolean(void *ctx, int is_true) 
{
        printf("%s,", is_true ? "true" : "false");
        return 0;
}

int do_null(void *ctx) 
{
        printf("null,");
        return 0;
}

int do_integer(void *ctx, int64_t l) 
{
        printf("%ld,", l);
        return 0;
}

int do_real(void *ctx, double d) 
{
        printf("%lf,", d);
        return 0;
}

int do_string(void *ctx, jsonn_string *s)
{
        printf("\"%s\"", s->bytes);
        return 0;
}

int do_key(void *ctx, jsonn_string *s) 
{
        printf("\"%s\":", s->bytes);
        return 0;
}

int do_begin_array(void *ctx)
{
        printf("[");
        return 0;
}

int do_end_array(void *ctx)
{
        printf("]");
        return 0;
}

int do_begin_object(void *ctx) 
{
        printf("{");
        return 0;
}

int do_end_object(void *ctx) 
{
        printf("}");
        return 0;
}

int do_error(void *ctx, jsonn_error *e)
{
        printf("\nError: %d [%ld]", e->code, e->at);
        return 0;
}


jsonn_callbacks callbacks = {
        .ctx = NULL,
        .j_boolean = do_boolean,
        .j_null = do_null,
        .j_integer = do_integer,
        .j_real = do_real,
        .j_string = do_string,
        .j_key = do_key,
        .j_begin_array = do_begin_array,
        .j_end_array = do_end_array,
        .j_begin_object = do_begin_object,
        .j_end_object = do_end_object,
        .j_error = do_error
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
        jsonn_type res = jsonn_parse(p, buf, len, &callbacks);
        if(res == JSONN_EOF)
                printf("\n\nResult EOF: %d\n", res);
        else
                printf("\n\nResult : %d (%d[%ld])\n", res, p->result.is.error.code, p->result.is.error.at);

        jsonn_free(p);


       
}


