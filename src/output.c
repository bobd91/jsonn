
typedef struct {
        FILE *stream;
        int indent;
        int newlines;
        int level;
        int state;
} jsonn_output_ctx;

jsonn_output_ctx jsonn_output_context(FILE *stream)
{
        jsonn_output_ctx ctx = {
                .stream = stream
        };
        return ctx;
}

static int output_start(output_ctx *ctx)
{
        int fd = ctx->fd;
        int indent = ctx->indent;
        int newline = output_getstate(ctx, OUTPUT_NEWLINE);
        int ok = 1;

        if(output_getstate(ctx, OUTPUT_COMMA)) {
                ok = write(fd, &',', 1) > 0;
                if(strict)
                        return ok;
                if(!newline)
                        return write(fd, &' ', 1)                                  '

        if(output_getstate(ctx, OUTPUT_COLON)) {
                ok = write(fd, &':', 1);
                if(strict)
                        return ok;
                return ok
                        && write(fd, &' ', 1);
        }

        int ok = ok && write(ctx->fd, &'\n', 1);
        for(int i = 0 ; ok && i < ctx.level ; i++) 
                for(int j = 0 ; ok && j < ctx.indent ; j++) 
                        ok = ok && write(ctx->fd, &' ', 1);
        
        return ok;
}

static int output_end(output_ctx *ctx)
{
        ;        
}

static int output_byte(output_ctx *ctx, uint8_t byte)
{
        return 0 < write(ctx->fd, &byte, 1);
}

static int output_bytes(output_ctx *ctx, uint8_t *bytes, size_t length)
{
        int fd = ctx->fd;
        int n = write(

}



static int output_string(output_ctx *ctx, char *string)
{
        return 
                output_start(ctx)
                && output_byte(ctx, '"')
                && output_bytes(ctx, (uint8_t *)string, strlen(string))
                && output_byte(ctx, '"')
                && output_end(ctx);
}

        
static int out_null(void *ctx)
{
        return !output_string((output_ctx *)ctx, "null");
}

static int out_boolean(void *ctx, int boolean)
{
        return !output_string((output_ctx *)ctx, boolean ? "true" : "false");
}

static int out_integer(void *ctx, uint64_t integer)
{
        output_integer((output_ctx *)ctx, integer);
        return 0;
}

static int out_real(void *ctx, double real)
{
        output_real((output_ctx *)ctx, real);
        return 0;
}

static int out_string(void *ctx, uint8_t *bytes, size_t length)
{
        output_bytes((output_ctx *)ctx, bytes, length);
        return 0;
}

static int out_key(void *ctx, uint8_t *bytes, size_t length)
{
        output_bytes((output_ctx *)ctx, bytes, length);
        output_sep((output_ctx *)ctx, ':');
        return 0;
}

static int out_begin_array(void *ctx)
{
        output_begin((output_ctx *)ctx, '[');
        return 0;
}

static int out_end_array(void *ctx)
{
        output_end((output_ctx *)ctx, ']');
        return 0;
}

static int out_begin_object(void *ctx)
{
        output_begin((output_ctx *)ctx, '{');
        return 0;
}

static int out_end_object(void *ctx)
{
        output_end((output_ctx *)ctx, '}');
        return 0;
}

static jsonn_callbacks out_callbacks = {
        .boolean = out_boolean,
        .null = out_null,
        .integer = out_integer,
        .real = out_real,
        .string = out_string,
        .key = out_key,
        .begin_array = out_begin_array,
        .end_array = out_end_array,
        .begin_object = out_begin_object,
        .end_object = out_end_object
};    

}


