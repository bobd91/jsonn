#include <assert.h>

static jsonpg_generator jsonpg_generator_new(
                jsonpg_callbacks *callbacks, 
                size_t ctx_size)
{
        jsonpg_generator g = jsonpg_alloc(sizeof(struct jsonpg_generator_s) + ctx_size);
        if(!g)
                return NULL;
        g->callbacks = callbacks;
        g->ctx = ctx_size
                ? (((void *)g) + sizeof(struct jsonpg_generator_s))
                : NULL;

        return g;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void jsonpg_generator_free(jsonpg_generator g)
{
        jsonpg_dealloc(g);
}
#pragma GCC diagnostic pop

static int jsonpg_null(jsonpg_generator g)
{
        return g->callbacks->null 
                && g->callbacks->null(g->ctx);
}

static int jsonpg_boolean(jsonpg_generator g, int is_true)
{
        return g->callbacks->boolean 
                && g->callbacks->boolean(g->ctx, is_true);
}

static int jsonpg_integer(jsonpg_generator g, int64_t integer)
{
        return g->callbacks->integer
                && g->callbacks->integer(g->ctx, integer);
}

static int jsonpg_real(jsonpg_generator g, double real)
{
        return g->callbacks->real 
                && g->callbacks->real(g->ctx, real);
}

static int jsonpg_string(jsonpg_generator g, uint8_t *bytes, size_t count)
{
        return g->callbacks->string
                && g->callbacks->string(g->ctx, bytes, count);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int jsonpg_cstr(jsonpg_generator g, char *cstr)
{
        return jsonpg_string(g, (uint8_t *)cstr, strlen(cstr));
}                        
#pragma GCC diagnostic pop

static int jsonpg_key(jsonpg_generator g, uint8_t *bytes, size_t count)
{
        return g->callbacks->key
                && g->callbacks->key(g->ctx, bytes, count);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int jsonpg_ckey(jsonpg_generator g, char *ckey)
{
        return jsonpg_string(g, (uint8_t *)ckey, strlen(ckey));
}
#pragma GCC diagnostic pop

static int jsonpg_begin_array(jsonpg_generator g)
{
        return g->callbacks->begin_array 
                && g->callbacks->begin_array(g->ctx);
}

static int jsonpg_end_array(jsonpg_generator g)
{
        return g->callbacks->end_array 
                && g->callbacks->end_array(g->ctx);
}

static int jsonpg_begin_object(jsonpg_generator g)
{
        return g->callbacks->begin_object 
                && g->callbacks->begin_object(g->ctx);
}

static int jsonpg_end_object(jsonpg_generator g)
{
        return g->callbacks->end_object 
                && g->callbacks->end_object(g->ctx);
}

static int jsonpg_error(jsonpg_generator g, int code, int at)
{
        (void)(g->callbacks->error 
                && g->callbacks->error(g->ctx, code, at));
        return 1; // always abort after error
}

static int generate(jsonpg_generator g, jsonpg_type type, jsonpg_value *value)
{
        switch(type) { 
        case JSONPG_NULL:
                return jsonpg_null(g);     
        case JSONPG_FALSE:
        case JSONPG_TRUE:
                return jsonpg_boolean(g, JSONPG_TRUE == type);
        case JSONPG_INTEGER:
                return jsonpg_integer(g, value->number.integer);
        case JSONPG_REAL:
                return jsonpg_real(g, value->number.real);
        case JSONPG_STRING:
                return jsonpg_string(g, value->string.bytes, value->string.length);
        case JSONPG_KEY:
                return jsonpg_key(g, value->string.bytes, value->string.length);
        case JSONPG_BEGIN_ARRAY:
                return jsonpg_begin_array(g);
        case JSONPG_END_ARRAY:
                return jsonpg_end_array(g);
        case JSONPG_BEGIN_OBJECT:
                return jsonpg_begin_object(g);
        case JSONPG_END_OBJECT:
                return jsonpg_end_object(g);
        case JSONPG_ERROR:
                return jsonpg_error(g, value->error.code, value->error.at);
        default:
                assert(!type && 0);
                return 1;
        }
}
