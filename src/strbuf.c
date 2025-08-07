#include <assert.h>

typedef struct str_buf_s *str_buf;

struct str_buf_s {
        block root;
        buffer current;
};

typedef struct buffer_copy_ctx_s {
        uint8_t *buf;
        int skip_first;
        int free;
} *buffer_copy_ctx;

void buffer_copy(void *item, void *vctx)
{
        buffer buf = item;
        buffer_copy_ctx ctx = vctx;
        if(ctx->skip_first) {
                ctx->skip_first = 0;
        } else {
                size_t count = buf->current - buf->start;
                memcpy(ctx->buf, buf->start, count);
                if(ctx->free)
                        jsonn_dealloc(buf->start);
                ctx->buf += count;
        }
}

void str_buf_cpy_ext(str_buf sbuf, uint8_t *bytes, int is_merge)
{
        assert(sbuf);
        assert(bytes);

        struct buffer_copy_ctx_s ctx = {
                .buf = bytes,
                .skip_first = is_merge,
                .free = is_merge
        };
        block_for_items(sbuf->root, buffer_copy, &ctx);
}

void str_buf_cpy(str_buf sbuf, uint8_t *bytes)
{
        str_buf_cpy_ext(sbuf, bytes, 0);
}

void str_buf_free(str_buf sbuf)
{
        assert(sbuf);

        if(sbuf->root)
                buffer_root_free(sbuf->root);
        jsonn_dealloc(sbuf);
}

str_buf str_buf_new()
{
        str_buf sbuf = jsonn_alloc(sizeof(struct str_buf_s));

        if(!sbuf)
                return NULL;

        sbuf->root = buffer_block_new();
        if(!sbuf->root) {
                str_buf_free(sbuf);
                return NULL;
        }

        sbuf->current = buffer_new(sbuf->root);
        if(!sbuf->current) {
                str_buf_free(sbuf);
                return NULL;
        }

        return sbuf;     
}


str_buf str_buf_append(str_buf sbuf, uint8_t *bytes, size_t count)
{
        assert(sbuf);

        uint8_t *from = bytes;
        size_t tocopy = count;

        buffer cbuf = sbuf->current;
        size_t space = cbuf->last - cbuf->current;
        while(tocopy) {
                if(space == 0) {
                        cbuf = sbuf->current = buffer_new(sbuf->root);
                        if(!cbuf)
                                return NULL;
                        sbuf->current = cbuf;
                        space = cbuf->last - cbuf->current;
                }
                size_t copy = (space < tocopy) ? space : tocopy;
                memcpy(cbuf->current, from, copy);
                cbuf->current += copy;
                from += copy;
                tocopy -= copy;
                space -= copy;
        }
        return sbuf;
}

void count_adder(void *item, void *ctx)
{
        buffer cbuf = item;
        size_t *count = ctx;
        *count += cbuf->current - cbuf->start;
}

size_t str_buf_count(str_buf sbuf)
{
        assert(sbuf);

        size_t count = 0;
        block_for_items(sbuf->root, count_adder, &count);
        return count;
}

size_t str_buf_merge(str_buf sbuf)
{
        assert(sbuf);

        // No need to merge if only one buffer
        block blk = sbuf->root;
        if(!(blk->next || 1 < item_count(blk)))
                return sbuf->current->current - sbuf->current->start;

        // Reallocate first buffer to hold all data
        size_t count = str_buf_count(sbuf);
        buffer cbuf = first_item(blk);
        uint8_t *mbuf = jsonn_realloc(cbuf->start, count);
        if(!mbuf)
                return 0;
        
        // Copy bytes from all other buffers into mbuf
        // Skip the first one and free any copied ones
        size_t size = cbuf->current - cbuf->start;
        str_buf_cpy_ext(sbuf, mbuf + size, 1);

        // First buffer now holds all the data
        cbuf->start = mbuf;
        cbuf->current = cbuf->last = mbuf + count;
        sbuf->current = cbuf;

        // Just the root block with one (big) buffer in it
        blk->slots_free = items_per_block(blk->item_size) - 1;
        blk->current = blk;
        block next = blk->next;
        blk->next = NULL;

        // Free any other blocks
        while(next) {
                blk = next;
                next = blk->next;
                jsonn_dealloc(blk);
        }

        return count;
}

size_t str_buf_content(str_buf sbuf, uint8_t **bytes)
{
        assert(sbuf);

        size_t count = str_buf_merge(sbuf);
        if(count < 0)
               return 0;
        *bytes = sbuf->current->start;
        return count;
}









