#include <stdio.h>
#include <fcntl.h>

#include "unity.c"

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


/*
*       static jsonn_visitor visitor = {
*               .callbacks = &callbacks,
*               .ctx = &pctx
*       };
*/      

void *talloc(size_t s)
{
        void *p = malloc(s);
        printf("\nAlloc  : %p\n", p);
        return p;
}

void tfree(void *p)
{
        printf("\nDealloc: %p\n", p);
        free(p);
        return;
}


jsonn_parser pp;

int main(int argc, char *argv[])
{
        //jsonn_set_allocator(talloc, tfree);

        jsonn_config c = jsonn_config_get();
        c.flags |=   JSONN_FLAG_COMMENTS
                   | JSONN_FLAG_TRAILING_COMMAS
                   | JSONN_FLAG_OPTIONAL_COMMAS
                   | JSONN_FLAG_UNQUOTED_KEYS
                   | JSONN_FLAG_UNQUOTED_STRINGS
                   | JSONN_FLAG_ESCAPE_CHARACTERS
                   | JSONN_FLAG_IS_ARRAY
                   ;

        jsonn_parser p = jsonn_new(NULL); //&c);

        pp = p;

        jsonn_type res;

        //      jsonn_visitor visitor = jsonn_stream_printer(stdout, 1);
        str_buf sbuf = str_buf_new();
        jsonn_visitor visitor = jsonn_buffer_printer(sbuf, 1);

        if(argc == 3 && 0 == strcmp("-e", argv[1])) {
                puts(argv[2]);
                uint8_t buf[1024];
                char *json = argv[2];
                size_t len = strlen(json);
                memcpy(buf, json, len + 1);
                res = jsonn_parse(p, buf, len, visitor);
        } else if(argc == 2) {
                int fd = open(argv[1], O_RDONLY, "rb");
                if(fd == -1) {
                        perror("Failed to open file");
                        exit(1);
                }

                res = jsonn_parse_fd(p, fd, visitor);
        }

        uint8_t *bytes;
        size_t count = str_buf_content(sbuf, &bytes);
        printf("%.*s\n", (int)count, bytes);

        if(res == JSONN_EOF)
                printf("\n\nResult EOF: %d\n", res);
        else
                printf("\n\nResult : %d (%d[%ld])\n", res, p->result.error.code, p->result.error.at);

        jsonn_visitor_free(visitor);
        jsonn_free(p);


       
}


