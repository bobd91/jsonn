typedef struct jsn_parser_s *jsn_parser;

typedef struct jsn_tree_s *jsn_tree;

typedef struct jsn_events_s {
        int null(jsn_ctx *);
        int boolean(jsn_ctx *, int);
        int integer(jsn_ctx *, int64_t);
        int real(jsn_ctx *, double);
        int key(jsn_ctx *, uint8_t *bytes, size_t count);
        int string(jsn_ctx *, uint8_t *bytes, size_t count);
        int begin_array(jsn_ctx *);
        int end_array(jsn_ctx *);
        int begin_object(jsn_ctx *);
        int end_object(jsn_ctx *);
        int eof(jsn_ctx *);

        int str(jsn_ctx, char *);
} jsn_events;

typedef struct {
        int errno;
        char *msg;
        int at;
} jsn_error;

typedef struct {
        int flags;
        jsn_events *events;
        void *ptr;
} jsn_ctx;




jsn_parser jsn_parser_new();
void jsn_parser_free(jsn_parser p);


int jsn_bytes_parse(jsn_parser p, uint8_t bytes, size_t count, jsn_ctx *ctx);
int jsn_file_parse(jsn_parser p, int fd, jsn_ctx *ctx);
int jsn_stream_parse(jsn_parser p, FILE *stream, jsn_ctx *ctx);

jsn_ctx jsn_file_write(int fd, int prettify);
jsn_ctx jsn_steam_write(FILE *stream, int prettify);
jsn_ctx jsn_tree_build();

int jsn_tree_read(jsn_tree tree, jsn_ctx *ctx); 
void jsn_tree_free(jsn_tree tree);

jsn_ctx jsn_context(jsn_events *events, void *ptr);

jsn_error jsn_parser_error(jsn_parser p);
jsn_error jsn_context_error(jsn_ctx ctx);

int jsn_file_parse(jsn_parser p, int fd, jsn_ctx *ctx)
{
        jsn_ctx->ptr = jsn_file_read_ctx(p->allocator, fd);
        return parse_buf(p, ctx);
}

int jsn_stream_parse(jsn_parser p, FILE *stream, jsn_ctx *ctx)
{
        return jsn_file_parse(p, fileno(stream), ctx);
}

int jsn_parse(jsn_parser p, jsn_events *custom_events) 
{
        return jsn_stream_parse(
                        p, 
                        stdin, 
                        jsn_context(custom_events, NULL));
}

jsn_ctx jsn_file_write(inf fd, int prettify)
{
        return jsn_context(jsn_file_events, jsn_file_write_ctx(fd, prettify));
}

jsn_ctx jsn_stream_write(FILE *stream, int prettify)
{
        return jsn_file_write(fileno(stream), prettify);
}

jsn_ctx jsn_context(jsn_events *events, void *ptr) {
        jsn_ctx ctx = {
                .events = jsn_monitor_events,
                .ptr = jsn_monitor(events, ptr)
        };
        return ctx;
}


                        




/*
 * jsn_ctx ctx = jsn_stream_write(stdout, 0);
 * ctx.events->begin_object(ctx);
 * ctx.events->str(ctx, "key1");
 * ctx.events->begin_array(ctx);
 * ctx.events->integer(ctx, 55);
 * ctx.events->boolean(ctx, 0);
 * ctx.events->cstr(ctx, "Hello");
 * ctx.events->end_array(ctx);
 * ctx.events->str(ctx, "key2");
 * ctx.events->real(ctx, 9.5);
 * ctx.events->end_object(ctx);
 * ctx.events->eof(ctx);  // frees ctx
 *
 * => '{"key1":[55,false,"Hello"],"key2":9.5}'
 *
 * Lots of boilerplate, ideal for macros
 * Based on i3's macros for using yajl_:wqgen
 *
 * j_ctx(stream_write, stdout, 0);
 * j_begin_object();
 * j_str("key2");
 * j_begin_array();
 * j_integer(55);
 * j_boolean(0);
 * j_str("Hello");
 * j_end_array();
 * j_str("key");
 * j_real(9.5);
 * j_end_object();
 * j_eof();
 * 
 * To disable macro generation:
 *
 * #define JSN_NO_MACROS
 * #include "jsn.h"
 */

