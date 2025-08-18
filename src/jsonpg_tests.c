#include <stdio.h>
#include <fcntl.h>

#include "jsonpg.c"


/*
*       static jsonpg_generator generator = {
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


jsonpg_parser pp;

int main(int argc, char *argv[])
{
        //jsonpg_set_allocator(talloc, tfree);

        jsonpg_config c = jsonpg_config_get();
        c.flags |=   JSONPG_FLAG_COMMENTS
                   | JSONPG_FLAG_TRAILING_COMMAS
                   | JSONPG_FLAG_OPTIONAL_COMMAS
                   | JSONPG_FLAG_UNQUOTED_KEYS
                   | JSONPG_FLAG_UNQUOTED_STRINGS
                   | JSONPG_FLAG_ESCAPE_CHARACTERS
                   | JSONPG_FLAG_IS_ARRAY
                   ;

        jsonpg_parser p = jsonpg_parser_new(NULL); //&c);

        pp = p;

        jsonpg_type res;

        //      jsonpg_generator generator = jsonpg_stream_printer(stdout, 1);
        str_buf sbuf = str_buf_new();
        jsonpg_generator generator = jsonpg_buffer_printer(sbuf, 1);

        if(argc == 3 && 0 == strcmp("-e", argv[1])) {
                puts(argv[2]);
                uint8_t buf[1024];
                char *json = argv[2];
                size_t len = strlen(json);
                memcpy(buf, json, len + 1);
                res = jsonpg_parse(p, buf, len, generator);
        } else if(argc == 2) {
                int fd = open(argv[1], O_RDONLY, "rb");
                if(fd == -1) {
                        perror("Failed to open file");
                        exit(1);
                }

                res = jsonpg_parse_fd(p, fd, generator);
        }

        uint8_t *bytes;
        size_t count = str_buf_content(sbuf, &bytes);
        printf("%.*s\n", (int)count, bytes);

        if(res == JSONPG_EOF)
                printf("\n\nResult EOF: %d\n", res);
        else
                printf("\n\nResult : %d (%d[%ld])\n", res, p->result.error.code, p->result.error.at);

        jsonpg_generator_free(generator);
        jsonpg_parser_free(p);


       
}


