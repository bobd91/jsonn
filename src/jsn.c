
typedef struct jsn_parser_s *jsn_parser;
typedef struct jsn_context_s *jsn_context;
typedef struct jsn_events_s jsn_events;
typedef struct str_buf_s *str_buf;

struct jsn_parser_s {
};

struct jsn_context_s {
        jsn_events *events;
        str_buf buf;
        void *ctx;
};


struct str_buf_s {
        uint8_t *bytes;
        size_t count;
        size_t length;
};

struct jsn_events_s {
        int (*begin_map)(jsn_context);
        int (*end_map)(jsn_context);
        int (*begin_array)(jsn_context);
        int (*end_array)(jsn_context);
        int (*null)(jsn_context);
        int (*boolean)(jsn_context, int);
        int (*integer)(jsn_context, long);
        int (*real)(jsn_context, double);
        int (*map_key)(jsn_context, uint8_t *, size_t);
        int (*string)(jsn_context, uint8_t *, size_t);
};

jsn_parser jsn_parser_new(jsn_config *config);
void jsn_parser_free(jsn_parser parser);

jsn_context jsn_context_new(jsn_events *events, void *ctx)
{
        assert(events);

        jsn_context = jsn_alloc(sizeof(struct jsn_context_s));

        if(!jsn_context)
                return NULL;

        jsn_context->buf = str_buf_new(STR_BUF_SIZE);
        if(!jsn_context->buf) {
                jsn_context_free(jsn_context);
                return NULL;
        }

        jsn_context->events = events;
        jsn_context->ctx = ctx;

        return jsn_context;
}


void jsn_context_free(jsn_context ctx)
{
        assert(ctx);

        str_buf_free(ctx->buf);
        jsn_dealloc(ctx);
}

int jsn_parse(jsn_parser parser, uint8_t *json, size_t count, jsn_context ctx)
{
        assert(parser);
        assert(ctx);
        
        while(1) {
                switch(parse_next_state(parser)) {
                case JSN_NULL:
                        jsn_null(ctx);
                        break;

                case JSN_TRUE:
                        jsn_boolean(ctx, 1);
                        break;

                case JSN_FALSE:
                        jsn_boolean(ctx, 0);
                        break;

                case JSN_STRING_START:
                        break;

                case JSN_STRING_END:
                        jsn_string(ctx, ..., ...);
                        break;

                case JSN_STRING_ESCAPE:
                        parse_string_escapes();
                        break;

                case JSN_NUMBER_START:
                        parse_number();
                        jsn_integer/jsn_real
                        break;

                case JSN_MAP_START:

                case JSN_MAP_END:
                case JSN_ARRAY_START:
                case JSN_ARRAY_END:
                case JSN_BUFFER_END:


}

int jsn_begin_map(jsn_context);
int jsn_end_map(jsn_context);
int jsn_begin_array(jsn_context);
int jsn_end_array(jsn_context);
int jsn_null(jsn_context);
int jsn_boolean(jsn_context, int);
int jsn_integer(jsn_context, long);
int jsn_real(jsn_context, double);
int jsn_map_key(jsn_context, uint8_t *, size_t)
int jsn_string(jsn_context, uint8_t *, size_t);
int jsn_key(jsn_context, char *);
int jsn_str(jsn_context, char *);

str_buf str_buf_new(size_t length)
{
        str_buf buf = jsn_alloc(sizeof(struct str_buf_s));

        if(!buf)
                return NULL;

        str_buf->bytes = jsn_alloc(length);
        if(!str_buf->bytes) {
                jsn_dealloc(str_buf);
                return NULL;
        }

        str_buf->count = 0;
        str_buf->length = length;

        return str_buf;
}

void str_buf_free(str_buf buf)
{
        assert(buf);

        jsn_dealloc(str_buf->bytes);
        jsn_dealloc(str_buf);
}

str_buf str_buf_append(str_buf current_buf, uint8_t *bytes, size_t count)
{
        assert(current_buf);

        str_buf buf = current_buf;
        size_t overflow = count + buf->count - buf->length;
        if(overflow > 0) {
                size_t new_length = buf->length 
                        + STR_BUF_SIZE * (1 + overflow / buf->length);
                buf = jsn_realloc(buf, new_length);
                if(!buf)
                        return NULL;
                buf->length = new_length
        }
        memcpy(buf->bytes + buf->count, bytes, count);
        buf->count += count;

        return str_buf;
}







