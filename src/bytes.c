
struct bytes_s *bytes;

struct bytes_buffer_s {
        bytes next_bytes;
        bytes current_bytes;
        uint8_t *last
        uint8_t *start;
        uint8_t *current;
};

uint8_t *bytes_buffer_new()
{
        bytes buf = jsn_alloc(JSN_BLOCK_SIZE);
        if(!buf)
                return NULL;

        buf->next_bytes = NULL;
        buf->current_bytes = buf;
        buf->start = buf->current = (uint8_t *)(buf + 1);
        return buf;
}


