#include <assert.h>

static jpg_visitor jpg_visitor_new(jpg_callbacks *callbacks, size_t ctx_size)
{
        jpg_visitor v = jpg_alloc(sizeof(struct jpg_visitor_s) + ctx_size);
        if(!v)
                return NULL;
        v->callbacks = callbacks;
        v->ctx = ctx_size
                ? (((void *)v) + sizeof(struct jpg_visitor_s))
                : NULL;

        return v;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void jpg_visitor_free(jpg_visitor v)
{
        jpg_dealloc(v);
}
#pragma GCC diagnostic pop

static int jpg_null(jpg_visitor v)
{
        return v->callbacks->null 
                && v->callbacks->null(v->ctx);
}

static int jpg_boolean(jpg_visitor v, int is_true)
{
        return v->callbacks->boolean 
                && v->callbacks->boolean(v->ctx, is_true);
}

static int jpg_integer(jpg_visitor v, int64_t integer)
{
        return v->callbacks->integer
                && v->callbacks->integer(v->ctx, integer);
}

static int jpg_real(jpg_visitor v, double real)
{
        return v->callbacks->real 
                && v->callbacks->real(v->ctx, real);
}

static int jpg_string(jpg_visitor v, uint8_t *bytes, size_t count)
{
        return v->callbacks->string
                && v->callbacks->string(v->ctx, bytes, count);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int jpg_cstr(jpg_visitor v, char *cstr)
{
        return jpg_string(v, (uint8_t *)cstr, strlen(cstr));
}                        
#pragma GCC diagnostic pop

static int jpg_key(jpg_visitor v, uint8_t *bytes, size_t count)
{
        return v->callbacks->key
                && v->callbacks->key(v->ctx, bytes, count);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int jpg_ckey(jpg_visitor v, char *ckey)
{
        return jpg_string(v, (uint8_t *)ckey, strlen(ckey));
}
#pragma GCC diagnostic pop

static int jpg_begin_array(jpg_visitor v)
{
        return v->callbacks->begin_array 
                && v->callbacks->begin_array(v->ctx);
}

static int jpg_end_array(jpg_visitor v)
{
        return v->callbacks->end_array 
                && v->callbacks->end_array(v->ctx);
}

static int jpg_begin_object(jpg_visitor v)
{
        return v->callbacks->begin_object 
                && v->callbacks->begin_object(v->ctx);
}

static int jpg_end_object(jpg_visitor v)
{
        return v->callbacks->end_object 
                && v->callbacks->end_object(v->ctx);
}

static int jpg_error(jpg_visitor v, int code, int at)
{
        (void)(v->callbacks->error 
                && v->callbacks->error(v->ctx, code, at));
        return 1; // always abort after error
}

static int visit(jpg_visitor visitor, jpg_type type, jpg_value *value)
{
        switch(type) { 
        case JPG_NULL:
                return jpg_null(visitor);     
        case JPG_FALSE:
        case JPG_TRUE:
                return jpg_boolean(visitor, JPG_TRUE == type);
        case JPG_INTEGER:
                return jpg_integer(visitor, value->number.integer);
        case JPG_REAL:
                return jpg_real(visitor, value->number.real);
        case JPG_STRING:
                return jpg_string(visitor, value->string.bytes, value->string.length);
        case JPG_KEY:
                return jpg_key(visitor, value->string.bytes, value->string.length);
        case JPG_BEGIN_ARRAY:
                return jpg_begin_array(visitor);
        case JPG_END_ARRAY:
                return jpg_end_array(visitor);
        case JPG_BEGIN_OBJECT:
                return jpg_begin_object(visitor);
        case JPG_END_OBJECT:
                return jpg_end_object(visitor);
        case JPG_ERROR:
                return jpg_error(visitor, value->error.code, value->error.at);
        default:
                assert(!type && 0);
                return 1;
        }
}
