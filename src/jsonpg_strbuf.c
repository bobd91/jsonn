#include <assert.h>

typedef struct str_buf_s *str_buf;

struct str_buf_s {
        uint8_t *bytes;
        uint32_t count;
        uint32_t size;    
};

str_buf str_buf_new()
{
        str_buf sbuf = jsonpg_alloc(sizeof(struct str_buf_s));
        if(!sbuf)
                return NULL;
        sbuf->bytes = jsonpg_alloc(JSONPG_BLOCK_SIZE);
        if(!sbuf->bytes) {
                jsonpg_dealloc(sbuf);
                return NULL;
        }
        sbuf->count = 0;
        sbuf->size = JSONPG_BLOCK_SIZE;

        return sbuf;
}

void str_buf_free(str_buf sbuf)
{
        assert(sbuf);

        jsonpg_dealloc(sbuf->bytes);
        jsonpg_dealloc(sbuf);
}

str_buf str_buf_append(str_buf sbuf, uint8_t *bytes, size_t count)
{
        assert(sbuf);

        int new_count = sbuf->count + count;

        if(new_count > sbuf->size) {
                do {
                        sbuf->size <<= 1;
                } while(new_count > sbuf->size);
                uint8_t *b = jsonpg_realloc(sbuf->bytes, sbuf->size);
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









