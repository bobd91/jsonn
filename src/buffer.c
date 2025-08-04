#include <unistd.h>

#define JSONN_BUFFER_SIZE 4096

typedef struct buffer_s {
        uint8_t *start;
        uint8_t *current;
        uint8_t *last;
} *buffer;

static block buffer_block_new()
{
        return block_new(sizeof(struct buffer_s));
}

static buffer buffer_new(block root_block)
{
        buffer b = block_item_new(root_block);
        b->start = jsonn_alloc(JSONN_BUFFER_SIZE);
        if(!b->start)
                return NULL;

        b->current = b->start;
        b->last = b->start + JSONN_BUFFER_SIZE - 1;

        return b;
}

static void buffer_free(void *item)
{
        buffer b = item;
        jsonn_dealloc(b->start);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void jsonn_buffer_root_free(block buffer_root)
{
        if(buffer_root) {
                block_for_items(buffer_root, buffer_free);
                block_root_free(buffer_root);
        }
}
#pragma GCC diagnostic pop

static int buffer_fill(jsonn_parser p, buffer buf)
{
        int max = buf->last - buf->current;
        uint8_t *pos = buf->current;
        while(max) {
                int l = read(p->fd, pos, max);
                if(l < 0) 
                        return -1;
                if(!l)
                        break;

                pos += l;
                max -= l;

        }
        buf->last -= max;
        buf->current = buf->start;

        return max == 0;

}

// Ensure that we have count bytes together in this block or the next
// Returns current position in the new block
// Returns NULL if failed to allocate new block
// or read fails
static uint8_t *ensure_current_prefix(jsonn_parser p, int count, uint8_t prefix)
{
        if(!p->seen_eof && count > p->last - p->current) {
                buffer buf = buffer_new(p->buffer_root);
                if(!buf) {
                        // TODO improve error reporting
                        return NULL;
                }
                if(prefix)
                        *buf->current++ = prefix;
                while(p->current < p->last)
                        *buf->current++ = *p->current++;
                int l = buffer_fill(p, buf);
                if(l < 0) {
                        // TODO improve error reporting
                        NULL;
                }

                p->start = buf->start;
                p->current = buf->current;
                p->last = buf->last;
                p->seen_eof = (l == 0);

        }
        return p->current;

}

static uint8_t *ensure_current(jsonn_parser p)
{
        return ensure_current_prefix(p, 1, '\0');
}

static uint8_t *ensure_current_n(jsonn_parser p, int count)
{
        return ensure_current_prefix(p, count, '\0');
}

// As the other ensure_... functions this ensures that we have
// count bytes together in this block or the next
// Rather than returning the current position in the new block
// Return 1 if characters are available in current block
//        0 if characters had to be moved to new block
//          or block allocation failed
static int ensure_string(jsonn_parser p, int count, uint8_t quote)
{
        return p->current == ensure_current_prefix(p, count, quote);
}



        



