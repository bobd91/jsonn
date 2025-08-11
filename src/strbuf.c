#include <assert.h>

typedef struct str_buf_s *str_buf;

struct str_buf_s {
        uint8_t *bytes;
        size_t count;
        size_t size;    
};

str_buf str_buf_new()
{
        str_buf sbuf = jsonn_alloc(sizeof(struct str_buf_s));
        if(!sbuf)
                return NULL;
        sbuf->bytes = jsonn_alloc(JSONN_BLOCK_SIZE);
        if(!sbuf->bytes) {
                jsonn_dealloc(sbuf);
                return NULL;
        }
        sbuf->count = 0;
        sbuf->size = JSONN_BLOCK_SIZE;

        return sbuf;
}

void str_buf_free(str_buf sbuf)
{
        assert(sbuf);

        jsonn_dealloc(sbuf->bytes);
        jsonn_dealloc(sbuf);
}

str_buf str_buf_append(str_buf sbuf, uint8_t *bytes, size_t count)
{
        assert(sbuf);

        int new_count = sbuf->count + count;

        if(new_count > sbuf->size) {
                do {
                        sbuf->size <<= 1;
                } while(new_count > sbuf->size);
                uint8_t *b = jsonn_realloc(sbuf->bytes, sbuf->size);
                if(!b)
                        return NULL;

                sbuf->bytes = b;
        }
        memcpy(sbuf->bytes + sbuf->count, bytes, count);
        sbuf->count += count;

        return sbuf;
}

str_buf str_buf_append_chars(str_buf sbuf, char *str)
{
        return str_buf_append(sbuf, (uint8_t *)str, strlen(str));
}

size_t str_buf_content(str_buf sbuf, uint8_t **bytes)
{
        assert(sbuf);

        *bytes = sbuf->bytes;
        return sbuf->count;
}









