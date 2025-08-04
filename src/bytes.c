
#define JSN_BYTE_BUFFER_SIZE 4096

typedef struct byte_buffer_s {
        uint8_t *start;
        uint8_t *current;
        uint8_t *last
} *byte_buffer;

block byte_buffer_block_new()
{
        return block_new(sizeof(byte_buffer_s));
}

byte_buffer byte_buffer_new(block root_block)
{
        byte_buffer b = block_item_new(root_block);
        b->start = jsn_alloc(JSN_BYTE_BUFFER_SIZE);
        if(!b->start)
                return NULL;

        b->current = b->start;
        b->last = b->start + JSN_BYTE_BUFFER_SIZE;

        return b;
}

void byte_buffer_free(void *item)
{
        byte_buffer b = item;
        jsn_dealloc(b->start);
}

void byte_buffer_block_free(block root_block)
{
        block_for_items(block, byte_buffer_free);
        block_free(block);
}

        



