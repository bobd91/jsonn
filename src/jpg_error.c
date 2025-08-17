#include <stdio.h>

static void dump_p(jpg_parser p)
{
        fprintf(stderr, "Parser Error:\n");
        fprintf(stderr, "Error: %d\n", p->result.error.code);
        fprintf(stderr, "At Position: %ld\n", p->result.error.at);
        fprintf(stderr, "Input Length: %ld\n", p->last - p->start);
        fprintf(stderr, "Input Processed: %ld\n", p->current - p->start);
        fprintf(stderr, "Next State: %d\n", p->next);
        fprintf(stderr, "Stack Size: %ld\n", p->stack_size);
        fprintf(stderr, "Stack Pointer: %ld\n", p->stack_pointer);
        fprintf(stderr, "Stack: ");
        for(int i = 0 ; i < p->stack_pointer ; i++) {
                int offset = i >> 3;
                int mask = 1 << (i & 0x07);
                fprintf(stderr, "%c", 
                                (mask & p->stack[offset]) ? '[' : '{');
        }
        fprintf(stderr, "\n");

}

static jpg_type error(jpg_parser p, jpg_error_code code) 
{
        p->result.error.code = code;
        p->result.error.at = p->current - p->start;

        dump_p(p);

        return JPG_ERROR;
}

static jpg_type parse_error(jpg_parser p)
{
        return error(p, JPG_ERROR_PARSE);
}

static jpg_type number_error(jpg_parser p)
{
        return error(p, JPG_ERROR_NUMBER);
}

static jpg_type utf8_error(jpg_parser p)
{
        return error(p, JPG_ERROR_UTF8);
}

static jpg_type alloc_error(jpg_parser p)
{
        return error(p, JPG_ERROR_ALLOC);
}

static jpg_type file_read_error(jpg_parser p)
{
        return error(p, JPG_ERROR_FILE_READ);
}


