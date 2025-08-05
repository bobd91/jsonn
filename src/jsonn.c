#include <stdlib.h>
#include <string.h>


void jsonn_set_allocator(void *(malloc)(size_t), void (*free)(void *))
{
        jsonn_alloc = malloc;
        jsonn_dealloc = free;
}

jsonn_parser jsonn_new(jsonn_config *config)
{
        jsonn_config c = config_select(config); 

        size_t struct_bytes = sizeof(struct jsonn_parser_s);
        // 1-8 => 1, 9-16 => 2, etc
        size_t stack_bytes = (c.stack_size + 7) / 8;
        jsonn_parser p = jsonn_alloc(struct_bytes + stack_bytes);
        if(p) {
                p->stack_size = c.stack_size;
                p->stack = (uint8_t *)(p + struct_bytes);
                p->flags = c.flags;
                p->repeat_next = PARSE_NEXT;
        }
        return p;
}

void jsonn_free(jsonn_parser p) 
{
        if(p) jsonn_dealloc(p);
}

jsonn_type jsonn_parse(
                jsonn_parser p, 
                uint8_t *json, 
                size_t length,
                jsonn_visitor *visitor)
{
        p->next = jsonn_init_next(p);
        p->start = p->current = p->write = json;
        p->last = json + length;
        p->seen_eof = 1;
        *p->last = '\0';
        p->stack_pointer = 0;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return visitor
                ? parse_visit(p, visitor)
                : JSONN_ROOT;
}

jsonn_type jsonn_parse_fd(jsonn_parser p, int fd, jsonn_visitor *visitor)
{
        p->next = jsonn_init_next(p);
        p->fd = fd;
        p->buffer_root = buffer_block_new();
        if(!p->buffer_root)
                return alloc_error(p);
        buffer buff = buffer_new(p->buffer_root);
        if(!buff)
                return alloc_error(p);
        int res = buffer_fill(p, buff);
        if(res == -1)
                return file_read_error(p);
        p->seen_eof = (0 == res);
        p->start = p->current = buff->start;
        p->last = buff->last;
        *p->last = '\0';
        p->stack_pointer = 0;

        // Skip leading byte order mark
        p->current += bom_bytes(p);

        return visitor
                ? parse_visit(p, visitor)
                : JSONN_ROOT;
}

void jsonn_parse_start(
                jsonn_parser p,
                uint8_t *json,
                size_t length)
{
        jsonn_parse(p, json, length, NULL);
}

jsonn_type jsonn_parse_next(jsonn_parser p)
{
        return jsonn_next(p);
}
